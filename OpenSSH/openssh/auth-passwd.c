/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Password authentication.  This file contains the functions to check whether
 * the password is valid for the user.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999 Dug Song.  All rights reserved.
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
 *
 *
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: auth-passwd.c,v 1.18 2000/10/03 18:03:03 markus Exp $");

#if !defined(USE_PAM) && !defined(HAVE_OSF_SIA)

#include "packet.h"
#include "ssh.h"
#include "servconf.h"
#include "xmalloc.h"

#ifdef WITH_AIXAUTHENTICATE
# include <login.h>
#endif
#ifdef __hpux
# include <hpsecurity.h>
# include <prot.h>
#endif
#ifdef HAVE_SCO_PROTECTED_PW
# include <sys/security.h>
# include <sys/audit.h>
# include <prot.h>
#endif /* HAVE_SCO_PROTECTED_PW */
#if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
# include <shadow.h>
#endif
#if defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW)
# include <sys/label.h>
# include <sys/audit.h>
# include <pwdadj.h>
#endif
#if defined(HAVE_MD5_PASSWORDS) && !defined(HAVE_MD5_CRYPT)
# include "md5crypt.h"
#endif /* defined(HAVE_MD5_PASSWORDS) && !defined(HAVE_MD5_CRYPT) */

#ifdef HAVE_CYGWIN
#undef ERROR
#include <windows.h>
#include <sys/cygwin.h>
#define is_winnt       (GetVersion() < 0x80000000)
#endif

/*
 * Tries to authenticate the user using password.  Returns true if
 * authentication succeeds.
 */
int
auth_password(struct passwd * pw, const char *password)
{
	extern ServerOptions options;
	char *encrypted_password;
	char *pw_password;
	char *salt;
#ifdef __hpux
	struct pr_passwd *spw;
#endif
#ifdef HAVE_SCO_PROTECTED_PW
	struct pr_passwd *spw;
#endif /* HAVE_SCO_PROTECTED_PW */
#if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
	struct spwd *spw;
#endif
#if defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW)
	struct passwd_adjunct *spw;
#endif
#ifdef WITH_AIXAUTHENTICATE
	char *authmsg;
	char *loginmsg;
	int reenter = 1;
#endif

	/* deny if no user. */
	if (pw == NULL)
		return 0;
#ifndef HAVE_CYGWIN
	if (pw->pw_uid == 0 && options.permit_root_login == 2)
		return 0;
#endif
#ifdef HAVE_CYGWIN
	/*
	 * Empty password is only possible on NT if the user has _really_
	 * an empty password and authentication is done, though.
	 */
        if (!is_winnt) 
#endif
	if (*password == '\0' && options.permit_empty_passwd == 0)
		return 0;

#ifdef HAVE_CYGWIN
	if (is_winnt) {
		HANDLE hToken = cygwin_logon_user(pw, password);

		if (hToken == INVALID_HANDLE_VALUE)
			return 0;
		cygwin_set_impersonation_token(hToken);
		return 1;
	}
#endif

#ifdef SKEY_VIA_PASSWD_IS_DISABLED
	if (options.skey_authentication == 1) {
		int ret = auth_skey_password(pw, password);
		if (ret == 1 || ret == 0)
			return ret;
		/* Fall back to ordinary passwd authentication. */
	}
#endif

#ifdef WITH_AIXAUTHENTICATE
	return (authenticate(pw->pw_name,password,&reenter,&authmsg) == 0);
#endif

#ifdef KRB4
	if (options.kerberos_authentication == 1) {
		int ret = auth_krb4_password(pw, password);
		if (ret == 1 || ret == 0)
			return ret;
		/* Fall back to ordinary passwd authentication. */
	}
#endif


	pw_password = pw->pw_passwd;

	/*
	 * Various interfaces to shadow or protected password data
	 */
#if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
	spw = getspnam(pw->pw_name);
	if (spw != NULL) 
		pw_password = spw->sp_pwdp;
#endif /* defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW) */

#ifdef HAVE_SCO_PROTECTED_PW
	spw = getprpwnam(pw->pw_name);
	if (spw != NULL) 
		pw_password = spw->ufld.fd_encrypt;
#endif /* HAVE_SCO_PROTECTED_PW */

#if defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW)
	if (issecure() && (spw = getpwanam(pw->pw_name)) != NULL)
		pw_password = spw->pwa_passwd;
#endif /* defined(HAVE_GETPWANAM) && !defined(DISABLE_SHADOW) */

#if defined(__hpux)
	if (iscomsec() && (spw = getprpwnam(pw->pw_name)) != NULL)
		pw_password = spw->ufld.fd_encrypt;
#endif /* defined(__hpux) */

	/* Check for users with no password. */
	if ((password[0] == '\0') && (pw_password[0] == '\0'))
		return 1;

	if (pw_password[0] != '\0')
		salt = pw_password;
	else
		salt = "xx";

#ifdef HAVE_MD5_PASSWORDS
	if (is_md5_salt(salt))
		encrypted_password = md5_crypt(password, salt);
	else
		encrypted_password = crypt(password, salt);
#else /* HAVE_MD5_PASSWORDS */    
# ifdef __hpux
	if (iscomsec())
		encrypted_password = bigcrypt(password, salt);
	else
		encrypted_password = crypt(password, salt);
# else
	encrypted_password = crypt(password, salt);
# endif /* __hpux */
#endif /* HAVE_MD5_PASSWORDS */    

	/* Authentication is accepted if the encrypted passwords are identical. */
	return (strcmp(encrypted_password, pw_password) == 0);
}
#endif /* !USE_PAM && !HAVE_OSF_SIA */
