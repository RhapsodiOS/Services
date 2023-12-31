/*
 * Copyright (c) 2000 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef USE_PAM
#include "ssh.h"
#include "xmalloc.h"
#include "servconf.h"

RCSID("$Id: auth-pam.c,v 1.1.1.4 2000/12/06 21:56:10 wsanchez Exp $");

#define NEW_AUTHTOK_MSG \
	"Warning: Your password has expired, please change it now"

/* Callbacks */
static int pamconv(int num_msg, const struct pam_message **msg,
	  struct pam_response **resp, void *appdata_ptr);
void pam_cleanup_proc(void *context);
void pam_msg_cat(const char *msg);

/* module-local variables */
static struct pam_conv conv = {
	pamconv,
	NULL
};
static pam_handle_t *pamh = NULL;
static const char *pampasswd = NULL;
static char *pam_msg = NULL;

/* states for pamconv() */
typedef enum { INITIAL_LOGIN, OTHER } pamstates;
static pamstates pamstate = INITIAL_LOGIN;
/* remember whether pam_acct_mgmt() returned PAM_NEWAUTHTOK_REQD */
static int password_change_required = 0;

/*
 * PAM conversation function.
 * There are two states this can run in.
 *
 * INITIAL_LOGIN mode simply feeds the password from the client into
 * PAM in response to PAM_PROMPT_ECHO_OFF, and collects output
 * messages with pam_msg_cat().  This is used during initial
 * authentication to bypass the normal PAM password prompt.
 *
 * OTHER mode handles PAM_PROMPT_ECHO_OFF with read_passphrase(prompt, 1)
 * and outputs messages to stderr. This mode is used if pam_chauthtok()
 * is called to update expired passwords.
 */
static int pamconv(int num_msg, const struct pam_message **msg,
	struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *reply;
	int count;
	char buf[1024];

	/* PAM will free this later */
	reply = malloc(num_msg * sizeof(*reply));
	if (reply == NULL)
		return PAM_CONV_ERR; 

	for (count = 0; count < num_msg; count++) {
		switch ((*msg)[count].msg_style) {
			case PAM_PROMPT_ECHO_ON:
				if (pamstate == INITIAL_LOGIN) {
					free(reply);
					return PAM_CONV_ERR;
				} else {
					fputs((*msg)[count].msg, stderr);
					fgets(buf, sizeof(buf), stdin);
					reply[count].resp = xstrdup(buf);
					reply[count].resp_retcode = PAM_SUCCESS;
					break;
				}
			case PAM_PROMPT_ECHO_OFF:
				if (pamstate == INITIAL_LOGIN) {
					if (pampasswd == NULL) {
						free(reply);
						return PAM_CONV_ERR;
					}
					reply[count].resp = xstrdup(pampasswd);
				} else {
					reply[count].resp = 
						xstrdup(read_passphrase((*msg)[count].msg, 1));
				}
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				if ((*msg)[count].msg != NULL) {
					if (pamstate == INITIAL_LOGIN)
						pam_msg_cat((*msg)[count].msg);
					else {
						fputs((*msg)[count].msg, stderr);
						fputs("\n", stderr);
					}
				}
				reply[count].resp = xstrdup("");
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			default:
				free(reply);
				return PAM_CONV_ERR;
		}
	}

	*resp = reply;

	return PAM_SUCCESS;
}

/* Called at exit to cleanly shutdown PAM */
void pam_cleanup_proc(void *context)
{
	int pam_retval;

	if (pamh != NULL)
	{
		pam_retval = pam_close_session(pamh, 0);
		if (pam_retval != PAM_SUCCESS) {
			log("Cannot close PAM session[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}

		pam_retval = pam_setcred(pamh, PAM_DELETE_CRED);
		if (pam_retval != PAM_SUCCESS) {
			debug("Cannot delete credentials[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}

		pam_retval = pam_end(pamh, pam_retval);
		if (pam_retval != PAM_SUCCESS) {
			log("Cannot release PAM authentication[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}
}

/* Attempt password authentation using PAM */
int auth_pam_password(struct passwd *pw, const char *password)
{
	extern ServerOptions options;
	int pam_retval;

	/* deny if no user. */
	if (pw == NULL)
		return 0;
	if (pw->pw_uid == 0 && options.permit_root_login == 2)
		return 0;
	if (*password == '\0' && options.permit_empty_passwd == 0)
		return 0;

	pampasswd = password;
	
	pamstate = INITIAL_LOGIN;
	pam_retval = pam_authenticate(pamh, 0);
	if (pam_retval == PAM_SUCCESS) {
		debug("PAM Password authentication accepted for user \"%.100s\"", 
			pw->pw_name);
		return 1;
	} else {
		debug("PAM Password authentication for \"%.100s\" failed[%d]: %s", 
			pw->pw_name, pam_retval, PAM_STRERROR(pamh, pam_retval));
		return 0;
	}
}

/* Do account management using PAM */
int do_pam_account(char *username, char *remote_user)
{
	int pam_retval;
	
	debug("PAM setting rhost to \"%.200s\"", get_canonical_hostname());
	pam_retval = pam_set_item(pamh, PAM_RHOST, 
		get_canonical_hostname());
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM set rhost failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}

	if (remote_user != NULL) {
		debug("PAM setting ruser to \"%.200s\"", remote_user);
		pam_retval = pam_set_item(pamh, PAM_RUSER, remote_user);
		if (pam_retval != PAM_SUCCESS) {
			fatal("PAM set ruser failed[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}

	pam_retval = pam_acct_mgmt(pamh, 0);
	switch (pam_retval) {
		case PAM_SUCCESS:
			/* This is what we want */
			break;
		case PAM_NEW_AUTHTOK_REQD:
			pam_msg_cat(NEW_AUTHTOK_MSG);
			/* flag that password change is necessary */
			password_change_required = 1;
			break;
		default:
			log("PAM rejected by account configuration[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
			return(0);
	}
	
	return(1);
}

/* Do PAM-specific session initialisation */
void do_pam_session(char *username, const char *ttyname)
{
	int pam_retval;

	if (ttyname != NULL) {
		debug("PAM setting tty to \"%.200s\"", ttyname);
		pam_retval = pam_set_item(pamh, PAM_TTY, ttyname);
		if (pam_retval != PAM_SUCCESS) {
			fatal("PAM set tty failed[%d]: %.200s", 
				pam_retval, PAM_STRERROR(pamh, pam_retval));
		}
	}

	pam_retval = pam_open_session(pamh, 0);
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM session setup failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
}

/* Set PAM credentials */ 
void do_pam_setcred(void)
{
	int pam_retval;
 
	debug("PAM establishing creds");
	pam_retval = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM setcred failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
}

/* accessor function for file scope static variable */
int pam_password_change_required(void)
{
	return password_change_required;
}

/* 
 * Have user change authentication token if pam_acct_mgmt() indicated
 * it was expired.  This needs to be called after an interactive
 * session is established and the user's pty is connected to
 * stdin/stout/stderr.
 */
void do_pam_chauthtok(void)
{
	int pam_retval;

	if (password_change_required) {
		pamstate = OTHER;
		/*
		 * XXX: should we really loop forever?
		 */
		do {
			pam_retval = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
			if (pam_retval != PAM_SUCCESS) {
				log("PAM pam_chauthtok failed[%d]: %.200s", 
					pam_retval, PAM_STRERROR(pamh, pam_retval));
			}
		} while (pam_retval != PAM_SUCCESS);
	}
}

/* Cleanly shutdown PAM */
void finish_pam(void)
{
	pam_cleanup_proc(NULL);
	fatal_remove_cleanup(&pam_cleanup_proc, NULL);
}

/* Start PAM authentication for specified account */
void start_pam(struct passwd *pw)
{
	int pam_retval;

	debug("Starting up PAM with username \"%.200s\"", pw->pw_name);

	pam_retval = pam_start(SSHD_PAM_SERVICE, pw->pw_name, &conv, &pamh);

	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM initialisation failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}

#ifdef PAM_TTY_KLUDGE
	/*
	 * Some PAM modules (e.g. pam_time) require a TTY to operate,
	 * and will fail in various stupid ways if they don't get one. 
	 * sshd doesn't set the tty until too late in the auth process and may
	 * not even need one (for tty-less connections)
	 * Kludge: Set a fake PAM_TTY 
	 */
	pam_retval = pam_set_item(pamh, PAM_TTY, "ssh");
	if (pam_retval != PAM_SUCCESS) {
		fatal("PAM set tty failed[%d]: %.200s", 
			pam_retval, PAM_STRERROR(pamh, pam_retval));
	}
#endif /* PAM_TTY_KLUDGE */

	fatal_add_cleanup(&pam_cleanup_proc, NULL);
}

/* Return list of PAM enviornment strings */
char **fetch_pam_environment(void)
{
#ifdef HAVE_PAM_GETENVLIST
	return(pam_getenvlist(pamh));
#else /* HAVE_PAM_GETENVLIST */
	return(NULL);
#endif /* HAVE_PAM_GETENVLIST */
}

/* Print any messages that have been generated during authentication */
/* or account checking to stderr */
void print_pam_messages(void)
{
	if (pam_msg != NULL)
		fputs(pam_msg, stderr);
}

/* Append a message to the PAM message buffer */
void pam_msg_cat(const char *msg)
{
	char *p;
	size_t new_msg_len;
	size_t pam_msg_len;
	
	new_msg_len = strlen(msg);
	
	if (pam_msg) {
		pam_msg_len = strlen(pam_msg);
		pam_msg = xrealloc(pam_msg, new_msg_len + pam_msg_len + 2);
		p = pam_msg + pam_msg_len;
	} else {
		pam_msg = p = xmalloc(new_msg_len + 2);
	}

	memcpy(p, msg, new_msg_len);
	p[new_msg_len] = '\n';
	p[new_msg_len + 1] = '\0';
}

#endif /* USE_PAM */
