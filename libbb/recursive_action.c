/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#undef DEBUG_RECURS_ACTION

/*
 * Walk down all the directories under the specified
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * Unfortunately, while nftw(3) could replace this and reduce
 * code size a bit, nftw() wasn't supported before GNU libc 2.1,
 * and so isn't sufficiently portable to take over since glibc2.1
 * is so stinking huge.
 */

static int true_action(const char *fileName, struct stat *statbuf,
						void* userData, int depth)
{
	return TRUE;
}

/* fileAction return value of 0 on any file in directory will make
 * recursive_action() return 0, but it doesn't stop directory traversal
 * (fileAction/dirAction will be called on each file).
 *
 * if !depthFirst, dirAction return value of 0 (FALSE) or 2 (SKIP)
 * prevents recursion into that directory, instead
 * recursive_action() returns 0 (if FALSE) or 1 (if SKIP).
 *
 * followLinks=0/1 differs mainly in handling of links to dirs.
 * 0: lstat(statbuf). Calls fileAction on link name even if points to dir.
 * 1: stat(statbuf). Calls dirAction and optionally recurse on link to dir.
 */

int recursive_action(const char *fileName,
		unsigned flags,
		int (*fileAction)(const char *fileName, struct stat *statbuf, void* userData, int depth),
		int (*dirAction)(const char *fileName, struct stat *statbuf, void* userData, int depth),
		void* userData,
		const unsigned depth)
{
	struct stat statbuf;
	int status;
	DIR *dir;
	struct dirent *next;

	if (!fileAction) fileAction = true_action;
	if (!dirAction) dirAction = true_action;
	status = (flags & action_followLinks ? stat : lstat)(fileName, &statbuf);

	if (status < 0) {
#ifdef DEBUG_RECURS_ACTION
		bb_error_msg("status=%d followLinks=%d TRUE=%d",
				status, flags & action_followLinks, TRUE);
#endif
		goto done_nak_warn;
	}

	/* If S_ISLNK(m), then we know that !S_ISDIR(m).
	 * Then we can skip checking first part: if it is true, then
	 * (!dir) is also true! */
	if ( /* (!(flags & action_followLinks) && S_ISLNK(statbuf.st_mode)) || */
	 !S_ISDIR(statbuf.st_mode)
	) {
		return fileAction(fileName, &statbuf, userData, depth);
	}

	/* It's a directory (or a link to one, and followLinks is set) */

	if (!(flags & action_recurse)) {
		return dirAction(fileName, &statbuf, userData, depth);
	}

	if (!(flags & action_depthFirst)) {
		status = dirAction(fileName, &statbuf, userData, depth);
		if (!status) {
			goto done_nak_warn;
		}
		if (status == SKIP)
			return TRUE;
	}

	dir = opendir(fileName);
	if (!dir) {
		/* findutils-4.1.20 reports this */
		/* (i.e. it doesn't silently return with exit code 1) */
		/* To trigger: "find -exec rm -rf {} \;" */
		goto done_nak_warn;
	}
	status = TRUE;
	while ((next = readdir(dir)) != NULL) {
		char *nextFile;

		nextFile = concat_subpath_file(fileName, next->d_name);
		if (nextFile == NULL)
			continue;
		/* now descend into it, forcing recursion. */
		if (!recursive_action(nextFile, flags | action_recurse,
				fileAction, dirAction, userData, depth+1)) {
			status = FALSE;
		}
		free(nextFile);
	}
	closedir(dir);
	if (flags & action_depthFirst &&
		!dirAction(fileName, &statbuf, userData, depth)) {
			goto done_nak_warn;
	}

	if (!status)
		return FALSE;
	return TRUE;
done_nak_warn:
	bb_perror_msg("%s", fileName);
	return FALSE;
}
