/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Rhosts authentication.  This file contains code to check whether to admit
 * the login based on rhosts authentication.  This file also processes
 * /etc/hosts.equiv.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth-rhosts.c,v 1.16 2000/10/03 18:03:03 markus Exp $");

#include "packet.h"
#include "ssh.h"
#include "xmalloc.h"
#include "uidswap.h"
#include "servconf.h"

/*
 * This function processes an rhosts-style file (.rhosts, .shosts, or
 * /etc/hosts.equiv).  This returns true if authentication can be granted
 * based on the file, and returns zero otherwise.
 */

int
check_rhosts_file(const char *filename, const char *hostname,
		  const char *ipaddr, const char *client_user,
		  const char *server_user)
{
	FILE *f;
	char buf[1024];	/* Must not be larger than host, user, dummy below. */

	/* Open the .rhosts file, deny if unreadable */
	f = fopen(filename, "r");
	if (!f)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		/* All three must be at least as big as buf to avoid overflows. */
		char hostbuf[1024], userbuf[1024], dummy[1024], *host, *user, *cp;
		int negated;

		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (*cp == '#' || *cp == '\n' || !*cp)
			continue;

		/*
		 * NO_PLUS is supported at least on OSF/1.  We skip it (we
		 * don't ever support the plus syntax).
		 */
		if (strncmp(cp, "NO_PLUS", 7) == 0)
			continue;

		/*
		 * This should be safe because each buffer is as big as the
		 * whole string, and thus cannot be overwritten.
		 */
		switch (sscanf(buf, "%s %s %s", hostbuf, userbuf, dummy)) {
		case 0:
			packet_send_debug("Found empty line in %.100s.", filename);
			continue;
		case 1:
			/* Host name only. */
			strlcpy(userbuf, server_user, sizeof(userbuf));
			break;
		case 2:
			/* Got both host and user name. */
			break;
		case 3:
			packet_send_debug("Found garbage in %.100s.", filename);
			continue;
		default:
			/* Weird... */
			continue;
		}

		host = hostbuf;
		user = userbuf;
		negated = 0;

		/* Process negated host names, or positive netgroups. */
		if (host[0] == '-') {
			negated = 1;
			host++;
		} else if (host[0] == '+')
			host++;

		if (user[0] == '-') {
			negated = 1;
			user++;
		} else if (user[0] == '+')
			user++;

		/* Check for empty host/user names (particularly '+'). */
		if (!host[0] || !user[0]) {
			/* We come here if either was '+' or '-'. */
			packet_send_debug("Ignoring wild host/user names in %.100s.",
					  filename);
			continue;
		}
		/* Verify that host name matches. */
		if (host[0] == '@') {
			if (!innetgr(host + 1, hostname, NULL, NULL) &&
			    !innetgr(host + 1, ipaddr, NULL, NULL))
				continue;
		} else if (strcasecmp(host, hostname) && strcmp(host, ipaddr) != 0)
			continue;	/* Different hostname. */

		/* Verify that user name matches. */
		if (user[0] == '@') {
			if (!innetgr(user + 1, NULL, client_user, NULL))
				continue;
		} else if (strcmp(user, client_user) != 0)
			continue;	/* Different username. */

		/* Found the user and host. */
		fclose(f);

		/* If the entry was negated, deny access. */
		if (negated) {
			packet_send_debug("Matched negative entry in %.100s.",
					  filename);
			return 0;
		}
		/* Accept authentication. */
		return 1;
	}

	/* Authentication using this file denied. */
	fclose(f);
	return 0;
}

/*
 * Tries to authenticate the user using the .shosts or .rhosts file. Returns
 * true if authentication succeeds.  If ignore_rhosts is true, only
 * /etc/hosts.equiv will be considered (.rhosts and .shosts are ignored).
 */

int
auth_rhosts(struct passwd *pw, const char *client_user)
{
	extern ServerOptions options;
	char buf[1024];
	const char *hostname, *ipaddr;
	struct stat st;
	static const char *rhosts_files[] = {".shosts", ".rhosts", NULL};
	unsigned int rhosts_file_index;

	/* no user given */
	if (pw == NULL)
		return 0;
	/* Switch to the user's uid. */
	temporarily_use_uid(pw->pw_uid);
	/*
	 * Quick check: if the user has no .shosts or .rhosts files, return
	 * failure immediately without doing costly lookups from name
	 * servers.
	 */
	for (rhosts_file_index = 0; rhosts_files[rhosts_file_index];
	     rhosts_file_index++) {
		/* Check users .rhosts or .shosts. */
		snprintf(buf, sizeof buf, "%.500s/%.100s",
			 pw->pw_dir, rhosts_files[rhosts_file_index]);
		if (stat(buf, &st) >= 0)
			break;
	}
	/* Switch back to privileged uid. */
	restore_uid();

	/* Deny if The user has no .shosts or .rhosts file and there are no system-wide files. */
	if (!rhosts_files[rhosts_file_index] &&
	    stat("/etc/hosts.equiv", &st) < 0 &&
	    stat(SSH_HOSTS_EQUIV, &st) < 0)
		return 0;

	hostname = get_canonical_hostname();
	ipaddr = get_remote_ipaddr();

	/* If not logging in as superuser, try /etc/hosts.equiv and shosts.equiv. */
	if (pw->pw_uid != 0) {
		if (check_rhosts_file("/etc/hosts.equiv", hostname, ipaddr, client_user,
				      pw->pw_name)) {
			packet_send_debug("Accepted for %.100s [%.100s] by /etc/hosts.equiv.",
					  hostname, ipaddr);
			return 1;
		}
		if (check_rhosts_file(SSH_HOSTS_EQUIV, hostname, ipaddr, client_user,
				      pw->pw_name)) {
			packet_send_debug("Accepted for %.100s [%.100s] by %.100s.",
				      hostname, ipaddr, SSH_HOSTS_EQUIV);
			return 1;
		}
	}
	/*
	 * Check that the home directory is owned by root or the user, and is
	 * not group or world writable.
	 */
	if (stat(pw->pw_dir, &st) < 0) {
		log("Rhosts authentication refused for %.100s: no home directory %.200s",
		    pw->pw_name, pw->pw_dir);
		packet_send_debug("Rhosts authentication refused for %.100s: no home directory %.200s",
				  pw->pw_name, pw->pw_dir);
		return 0;
	}
	if (options.strict_modes &&
	    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
	     (st.st_mode & 022) != 0)) {
		log("Rhosts authentication refused for %.100s: bad ownership or modes for home directory.",
		    pw->pw_name);
		packet_send_debug("Rhosts authentication refused for %.100s: bad ownership or modes for home directory.",
				  pw->pw_name);
		return 0;
	}
	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw->pw_uid);

	/* Check all .rhosts files (currently .shosts and .rhosts). */
	for (rhosts_file_index = 0; rhosts_files[rhosts_file_index];
	     rhosts_file_index++) {
		/* Check users .rhosts or .shosts. */
		snprintf(buf, sizeof buf, "%.500s/%.100s",
			 pw->pw_dir, rhosts_files[rhosts_file_index]);
		if (stat(buf, &st) < 0)
			continue;

		/*
		 * Make sure that the file is either owned by the user or by
		 * root, and make sure it is not writable by anyone but the
		 * owner.  This is to help avoid novices accidentally
		 * allowing access to their account by anyone.
		 */
		if (options.strict_modes &&
		    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		     (st.st_mode & 022) != 0)) {
			log("Rhosts authentication refused for %.100s: bad modes for %.200s",
			    pw->pw_name, buf);
			packet_send_debug("Bad file modes for %.200s", buf);
			continue;
		}
		/* Check if we have been configured to ignore .rhosts and .shosts files. */
		if (options.ignore_rhosts) {
			packet_send_debug("Server has been configured to ignore %.100s.",
					  rhosts_files[rhosts_file_index]);
			continue;
		}
		/* Check if authentication is permitted by the file. */
		if (check_rhosts_file(buf, hostname, ipaddr, client_user, pw->pw_name)) {
			packet_send_debug("Accepted by %.100s.",
					  rhosts_files[rhosts_file_index]);
			/* Restore the privileged uid. */
			restore_uid();
			return 1;
		}
	}

	/* Restore the privileged uid. */
	restore_uid();
	return 0;
}
