/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp_fh.c `pkg-config fuse --cflags --libs` -lulockmgr -o fusexmp_fh
*/

/*
 * Copyright (c) 2006-2008 Amit Singh/Google Inc.
 */

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include <fuse.h>
#ifndef __APPLE__
#include <ulockmgr.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#ifndef __APPLE__
#include <sys/file.h> /* flock(2) */
#endif

#include <sys/param.h>

#ifdef __APPLE__

#include <fcntl.h>
#include <sys/vnode.h>

#if defined(_POSIX_C_SOURCE)
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
#endif

#include <sys/attr.h>

#define G_PREFIX			"org"
#define G_KAUTH_FILESEC_XATTR G_PREFIX  ".apple.system.Security"
#define A_PREFIX			"com"
#define A_KAUTH_FILESEC_XATTR A_PREFIX  ".apple.system.Security"
#define XATTR_APPLE_PREFIX		"com.apple."

#endif /* __APPLE__ */

#include "ulockmgr.h"

#ifndef LOG_ON
#define LOG_ON 0
#endif

#define log(fmt, ...) \
        do { if (LOG_ON) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#include <string>

std::string pad_path(const char *path) {
    std::string res("/Users/pbergen/blt/app/main/orig.core");
    res.append(path);
    return res;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
        int res;


        std::string pp(pad_path(path));
        log("GETATTR %s -> %s\n", path, pp.c_str());
        res = lstat(pp.c_str(), stbuf);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_fgetattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi)
{
        int res;

        (void) path;

        res = fstat(fi->fh, stbuf);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_access(const char *path, int mask)
{
        int res;

        std::string pp(pad_path(path));
        res = access(pp.c_str(), mask);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
        int res;

        std::string pp(pad_path(path));
        res = readlink(pp.c_str(), buf, size - 1);
        if (res == -1)
                return -errno;

        buf[res] = '\0';
        return 0;
}

struct xmp_dirp {
        DIR *dp;
        struct dirent *entry;
        off_t offset;
};

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
        int res;
        struct xmp_dirp *d = (xmp_dirp *)malloc(sizeof(struct xmp_dirp));
        if (d == NULL)
                return -ENOMEM;

        std::string pp(pad_path(path));
        d->dp = opendir(pp.c_str());
        if (d->dp == NULL) {
                res = -errno;
                free(d);
                return res;
        }
        d->offset = 0;
        d->entry = NULL;

        fi->fh = (unsigned long) d;
        return 0;
}

static inline struct xmp_dirp *get_dirp(struct fuse_file_info *fi)
{
        return (struct xmp_dirp *) (uintptr_t) fi->fh;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
        struct xmp_dirp *d = get_dirp(fi);

        (void) path;
        if (offset != d->offset) {
                seekdir(d->dp, offset);
                d->entry = NULL;
                d->offset = offset;
        }
        while (1) {
                struct stat st;
                off_t nextoff;

                if (!d->entry) {
                        d->entry = readdir(d->dp);
                        if (!d->entry)
                                break;
                }

                memset(&st, 0, sizeof(st));
                st.st_ino = d->entry->d_ino;
                st.st_mode = d->entry->d_type << 12;
                nextoff = telldir(d->dp);
                if (filler(buf, d->entry->d_name, &st, nextoff))
                        break;

                d->entry = NULL;
                d->offset = nextoff;
        }

        return 0;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
        struct xmp_dirp *d = get_dirp(fi);
        (void) path;
        closedir(d->dp);
        free(d);
        return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
        int res;

        std::string pp(pad_path(path));
        if (S_ISFIFO(mode))
                res = mkfifo(pp.c_str(), mode);
        else
                res = mknod(pp.c_str(), mode, rdev);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
        int res;

        std::string pp(pad_path(path));
        res = mkdir(pp.c_str(), mode);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_unlink(const char *path)
{
        int res;

        std::string pp(pad_path(path));
        res = unlink(pp.c_str());
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_rmdir(const char *path)
{
        int res;

        std::string pp(pad_path(path));
        res = rmdir(pp.c_str());
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
        int res;

        res = symlink(from, to);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_rename(const char *from, const char *to)
{
        int res;

        res = rename(from, to);
        if (res == -1)
                return -errno;

        return 0;
}

#ifdef __APPLE__

static int xmp_setvolname(const char *volname)
{
    (void)volname;
    return 0;
}

static int xmp_exchange(const char *path1, const char *path2,
                        unsigned long options)
{
        int res;
        std::string p1(pad_path(path1)), p2(pad_path(path2));
        res = exchangedata(p1.c_str(), p2.c_str(), options);
        if (res == -1)
                return -errno;

        return 0;
}

#endif /* __APPLE__ */

static int xmp_link(const char *from, const char *to)
{
        int res;

        std::string pfrom(pad_path(from)), pto(pad_path(to));
        log("LINK %s -> %s\n", pfrom.c_str(), pto.c_str());
        //res = link(from, to);
        /*log("LINK-RES %s -> %s = %d (errno %d)\n", pfrom.c_str(), pto.c_str(), res, errno);
        if (res == -1) {
            res = -errno;
            struct stat *ss = (struct stat*) malloc(sizeof(struct stat));
            memset(ss,0,sizeof(struct stat));
            int sr = stat(pfrom.c_str(), ss);
            log("FAIL LINK FROM: %d, %d, link %d, ino %llu, dev %d, %llu, %d, gen %d (%d) --- %s\n", ss->st_dev,
                ss->st_mode,
                ss->st_nlink,
                ss->st_ino,
                ss->st_rdev,
                ss->st_size,
                ss->st_flags,
                ss->st_gen, sr, pfrom.c_str());
            memset(ss,0,sizeof(struct stat));
            sr = stat(pto.c_str(), ss);
            log("FAIL LINK TO: %d, %d, link %d, ino %llu, dev %d, %llu, %d, gen %d (%d) --- %s\n", ss->st_dev,
                ss->st_mode,
                ss->st_nlink,
                ss->st_ino,
                ss->st_rdev,
                ss->st_size,
                ss->st_flags,
                ss->st_gen, sr, pto.c_str());
            free((void*)ss);
        sleep(10);
        */
        std::string cmd("ln ");
            cmd += pfrom;
        cmd += " ";
        cmd += pto;
        res=system(cmd.c_str());

        /* Why does this work, but not the link() stdc call???? */

        log("LINK SYSTEM (%s) = %d (%d)\n", cmd.c_str(), res, errno);

        return res;
}

#ifdef __APPLE__

static int xmp_fsetattr_x(const char *path, struct setattr_x *attr,
                          struct fuse_file_info *fi)
{
        int res;
        uid_t uid = -1;
        gid_t gid = -1;

        std::string pp(pad_path(path));
        if (SETATTR_WANTS_MODE(attr)) {
                res = lchmod(pp.c_str(), attr->mode);
                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_UID(attr))
                uid = attr->uid;

        if (SETATTR_WANTS_GID(attr))
                gid = attr->gid;

        if ((uid != -1) || (gid != -1)) {
                res = lchown(pp.c_str(), uid, gid);
                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_SIZE(attr)) {
                if (fi)
                        res = ftruncate(fi->fh, attr->size);
                else
                        res = truncate(pp.c_str(), attr->size);
                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_MODTIME(attr)) {
                struct timeval tv[2];
                if (!SETATTR_WANTS_ACCTIME(attr))
                        gettimeofday(&tv[0], NULL);
                else {
                        tv[0].tv_sec = attr->acctime.tv_sec;
                        tv[0].tv_usec = attr->acctime.tv_nsec / 1000;
                }
                tv[1].tv_sec = attr->modtime.tv_sec;
                tv[1].tv_usec = attr->modtime.tv_nsec / 1000;
                res = utimes(pp.c_str(), tv);
                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_CRTIME(attr)) {
                struct attrlist attributes;

                attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
                attributes.reserved = 0;
                attributes.commonattr = ATTR_CMN_CRTIME;
                attributes.dirattr = 0;
                attributes.fileattr = 0;
                attributes.forkattr = 0;
                attributes.volattr = 0;

                res = setattrlist(pp.c_str(), &attributes, &attr->crtime,
                                  sizeof(struct timespec), FSOPT_NOFOLLOW);

                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_CHGTIME(attr)) {
                struct attrlist attributes;

                attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
                attributes.reserved = 0;
                attributes.commonattr = ATTR_CMN_CHGTIME;
                attributes.dirattr = 0;
                attributes.fileattr = 0;
                attributes.forkattr = 0;
                attributes.volattr = 0;

                res = setattrlist(pp.c_str(), &attributes, &attr->chgtime,
                                  sizeof(struct timespec), FSOPT_NOFOLLOW);

                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_BKUPTIME(attr)) {
                struct attrlist attributes;

                attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
                attributes.reserved = 0;
                attributes.commonattr = ATTR_CMN_BKUPTIME;
                attributes.dirattr = 0;
                attributes.fileattr = 0;
                attributes.forkattr = 0;
                attributes.volattr = 0;

                res = setattrlist(pp.c_str(), &attributes, &attr->bkuptime,
                                  sizeof(struct timespec), FSOPT_NOFOLLOW);

                if (res == -1)
                        return -errno;
        }

        if (SETATTR_WANTS_FLAGS(attr)) {
                res = chflags(pp.c_str(), attr->flags);
                if (res == -1)
                        return -errno;
        }

        return 0;
}

static int xmp_setattr_x(const char *path, struct setattr_x *attr)
{
        std::string pp(pad_path(path));
        return xmp_fsetattr_x(pp.c_str(), attr, (struct fuse_file_info *)0);
}

static int xmp_chflags(const char *path, uint32_t flags)
{
        int res;

        std::string pp(pad_path(path));
        res = chflags(pp.c_str(), flags);

        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_getxtimes(const char *path, struct timespec *bkuptime,
                         struct timespec *crtime)
{
        int res = 0;
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved    = 0;
        attributes.commonattr  = 0;
        attributes.dirattr     = 0;
        attributes.fileattr    = 0;
        attributes.forkattr    = 0;
        attributes.volattr     = 0;



        struct xtimeattrbuf {
                uint32_t size;
                struct timespec xtime;
        } __attribute__ ((packed));


        struct xtimeattrbuf buf;

        attributes.commonattr = ATTR_CMN_BKUPTIME;
        std::string pp(pad_path(path));
        res = getattrlist(pp.c_str(), &attributes, &buf, sizeof(buf), FSOPT_NOFOLLOW);
        if (res == 0)
                (void)memcpy(bkuptime, &(buf.xtime), sizeof(struct timespec));
        else
                (void)memset(bkuptime, 0, sizeof(struct timespec));

        attributes.commonattr = ATTR_CMN_CRTIME;
        res = getattrlist(pp.c_str(), &attributes, &buf, sizeof(buf), FSOPT_NOFOLLOW);
        if (res == 0)
                (void)memcpy(crtime, &(buf.xtime), sizeof(struct timespec));
        else
                (void)memset(crtime, 0, sizeof(struct timespec));

        return 0;
}

static int xmp_setbkuptime(const char *path, const struct timespec *bkuptime)
{
        int res;

        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_BKUPTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        std::string pp(pad_path(path));
        res = setattrlist(pp.c_str(), &attributes, (void *)bkuptime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_setchgtime(const char *path, const struct timespec *chgtime)
{
        int res;

        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CHGTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        std::string pp(pad_path(path));
        res = setattrlist(pp.c_str(), &attributes, (void *)chgtime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_setcrtime(const char *path, const struct timespec *crtime)
{
        int res;

        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CRTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        std::string pp(pad_path(path));
        res = setattrlist(pp.c_str(), &attributes, (void *)crtime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1)
                return -errno;

        return 0;
}

#endif /* __APPLE__ */

static int xmp_chmod(const char *path, mode_t mode)
{
        int res;

        std::string pp(pad_path(path));
#ifdef __APPLE__
        res = lchmod(pp.c_str(), mode);
#else
        res = chmod(pp.c_str(), mode);
#endif
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
        int res;

        std::string pp(pad_path(path));
        res = lchown(pp.c_str(), uid, gid);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
        int res;

        std::string pp(pad_path(path));
        res = truncate(pp.c_str(), size);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
                         struct fuse_file_info *fi)
{
        int res;

        (void) path;

        res = ftruncate(fi->fh, size);
        if (res == -1)
                return -errno;

        return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
        int res;

        /* don't use utime/utimes since they follow symlinks */
        std::string pp(pad_path(path));
        res = utimensat(0, pp.c_str(), ts, AT_SYMLINK_NOFOLLOW);
        if (res == -1)
                return -errno;

        return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
        int fd;

        std::string pp(pad_path(path));
        fd = open(pp.c_str(), fi->flags, mode);
        log("CREATE-RES %s = %d (errno %d)\n", pp.c_str(), fd, errno);
        if (fd == -1)
                return -errno;

        fi->fh = fd;
        return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
        int fd;

        std::string pp(pad_path(path));
        fd = open(pp.c_str(), fi->flags);
        if (fd == -1)
                return -errno;

        fi->fh = fd;
        return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
        int res;

        (void) path;
        res = pread(fi->fh, buf, size, offset);
        if (res == -1)
                res = -errno;

        return res;
}

static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
                        size_t size, off_t offset, struct fuse_file_info *fi)
{
        struct fuse_bufvec *src;

        (void) path;

        src = (fuse_bufvec *) malloc(sizeof(struct fuse_bufvec));
        if (src == NULL)
                return -ENOMEM;

        *src = FUSE_BUFVEC_INIT(size);

        src->buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
        src->buf[0].fd = fi->fh;
        src->buf[0].pos = offset;

        *bufp = src;

        return 0;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
        int res;

        (void) path;
        res = pwrite(fi->fh, buf, size, offset);
        if (res == -1)
                res = -errno;

        return res;
}

static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
                     off_t offset, struct fuse_file_info *fi)
{
        struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

        (void) path;

        dst.buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
        dst.buf[0].fd = fi->fh;
        dst.buf[0].pos = offset;

        return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
        int res;

        std::string pp(pad_path(path));
        res = statvfs(pp.c_str(), stbuf);
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
        int res;

        (void) path;
        /* This is called from every close on an open file, so call the
           close on the underlying filesystem.	But since flush may be
           called multiple times for an open file, this must not really
           close the file.  This is important if used on a network
           filesystem like NFS which flush the data/metadata on close() */
        res = close(dup(fi->fh));
        if (res == -1)
                return -errno;

        return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
        (void) path;
        close(fi->fh);

        return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
        int res;
        (void) path;

#ifndef HAVE_FDATASYNC
        (void) isdatasync;
#else
        if (isdatasync)
                res = fdatasync(fi->fh);
        else
#endif
                res = fsync(fi->fh);
        if (res == -1)
                return -errno;

        return 0;
}

#if defined(HAVE_POSIX_FALLOCATE) || defined(__APPLE__)
static int xmp_fallocate(const char *path, int mode,
                        off_t offset, off_t length, struct fuse_file_info *fi)
{
#ifdef __APPLE__
        fstore_t fstore;

        if (!(mode & PREALLOCATE))
                return -ENOTSUP;

        fstore.fst_flags = 0;
        if (mode & ALLOCATECONTIG)
                fstore.fst_flags |= F_ALLOCATECONTIG;
        if (mode & ALLOCATEALL)
                fstore.fst_flags |= F_ALLOCATEALL;

        if (mode & ALLOCATEFROMPEOF)
                fstore.fst_posmode = F_PEOFPOSMODE;
        else if (mode & ALLOCATEFROMVOL)
                fstore.fst_posmode = F_VOLPOSMODE;

        fstore.fst_offset = offset;
        fstore.fst_length = length;

        if (fcntl(fi->fh, F_PREALLOCATE, &fstore) == -1)
                return -errno;
        else
                return 0;
#else
        (void) path;

        if (mode)
                return -EOPNOTSUPP;

        return -posix_fallocate(fi->fh, offset, length);
#endif
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
#ifdef __APPLE__
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags, uint32_t position)
#else
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
#endif
{
        std::string pp(pad_path(path));
#ifdef __APPLE__
        int res;
        if (!strncmp(name, XATTR_APPLE_PREFIX, sizeof(XATTR_APPLE_PREFIX) - 1)) {
                flags &= ~(XATTR_NOSECURITY);
        }
        if (!strcmp(name, A_KAUTH_FILESEC_XATTR)) {
                char new_name[MAXPATHLEN];
                memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
                memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
                res = setxattr(pp.c_str(), new_name, value, size, position, flags);
        } else {
                res = setxattr(pp.c_str(), name, value, size, position, flags);
        }
#else
        int res = lsetxattr(pp.c_str(), name, value, size, flags);
#endif
        if (res == -1)
                return -errno;
        return 0;
}

#ifdef __APPLE__
static int xmp_getxattr(const char *path, const char *name, char *value,
                        size_t size, uint32_t position)
#else
static int xmp_getxattr(const char *path, const char *name, char *value,
                        size_t size)
#endif
{
        std::string pp(pad_path(path));
#ifdef __APPLE__
        int res;
        if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {
                char new_name[MAXPATHLEN];
                memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
                memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
                res = getxattr(pp.c_str(), new_name, value, size, position, XATTR_NOFOLLOW);
        } else {
                res = getxattr(pp.c_str(), name, value, size, position, XATTR_NOFOLLOW);
        }
#else
        int res = lgetxattr(pp.c_str(), name, value, size);
#endif
        if (res == -1)
                return -errno;
        return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
        std::string pp(pad_path(path));
#ifdef __APPLE__
        ssize_t res = listxattr(pp.c_str(), list, size, XATTR_NOFOLLOW);
        if (res > 0) {
                if (list) {
                        size_t len = 0;
                        char *curr = list;
                        do {
                                size_t thislen = strlen(curr) + 1;
                                if (strcmp(curr, G_KAUTH_FILESEC_XATTR) == 0) {
                                        memmove(curr, curr + thislen, res - len - thislen);
                                        res -= thislen;
                                        break;
                                }
                                curr += thislen;
                                len += thislen;
                        } while (len < res);
                } else {
                        /*
                        ssize_t res2 = getxattr(pp.c_str(), G_KAUTH_FILESEC_XATTR, NULL, 0, 0,
                                                XATTR_NOFOLLOW);
                        if (res2 >= 0) {
                                res -= sizeof(G_KAUTH_FILESEC_XATTR);
                        }
                        */
                }
        }
#else
        int res = llistxattr(pp.c_str(), list, size);
#endif
        if (res == -1)
                return -errno;
        return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
        std::string pp(pad_path(path));
#ifdef __APPLE__
        int res;
        if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {
                char new_name[MAXPATHLEN];
                memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
                memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);
                res = removexattr(pp.c_str(), new_name, XATTR_NOFOLLOW);
        } else {
                res = removexattr(pp.c_str(), name, XATTR_NOFOLLOW);
        }
#else
        int res = lremovexattr(pp.c_str(), name);
#endif
        if (res == -1)
                return -errno;
        return 0;
}
#endif /* HAVE_SETXATTR */

#ifndef __APPLE__LOCK
static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
                    struct flock *lock)
{
        std::string pp(pad_path(path));
        log("LOCK %s (%llu)\n", pp.c_str(), fi->fh);

        int res = ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
                           sizeof(fi->lock_owner));
        log("LOCK-RES %s = %d\n", pp.c_str(), res);
        return res;
}
#else
static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
                    struct flock *lock)
{
        std::string pp(pad_path(path));
        log("LOCK %s (%llu)\n", pp.c_str(), fi->fh);

        int res = ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
                           sizeof(fi->lock_owner));
        log("LOCK-RES %s = %d\n", pp.c_str(), res);
        return res;
}
#endif

void *
xmp_init(struct fuse_conn_info *conn)
{
#ifdef __APPLE__
        FUSE_ENABLE_SETVOLNAME(conn);
        FUSE_ENABLE_XTIMES(conn);
#endif
        return NULL;
}

void
xmp_destroy(void *userdata)
{
}

#ifndef __APPLE__LOCK
static int xmp_flock(const char *path, struct fuse_file_info *fi, int op)
{
        int res;
        //        (void) path;
        std::string pp(pad_path(path));

        log("FLOCK %s (%llu)\n", pp.c_str(), fi->fh);
        res = flock(fi->fh, op);
        if (res == -1)
                return -errno;

        return 0;
}
#endif

static struct fuse_operations xmp_oper = {
        .init           = xmp_init,
        .destroy	= xmp_destroy,
        .getattr	= xmp_getattr,
        .fgetattr	= xmp_fgetattr,
#ifndef __APPLE__
        .access		= xmp_access,
#endif
        .readlink	= xmp_readlink,
        .opendir	= xmp_opendir,
        .readdir	= xmp_readdir,
        .releasedir	= xmp_releasedir,
        .mknod		= xmp_mknod,
        .mkdir		= xmp_mkdir,
        .symlink	= xmp_symlink,
        .unlink		= xmp_unlink,
        .rmdir		= xmp_rmdir,
        .rename		= xmp_rename,
        .link		= xmp_link,
        .chmod		= xmp_chmod,
        .chown		= xmp_chown,
        .truncate	= xmp_truncate,
        .ftruncate	= xmp_ftruncate,
#ifdef HAVE_UTIMENSAT
        .utimens	= xmp_utimens,
#endif
        .create		= xmp_create,
        .open		= xmp_open,
        .read		= xmp_read,
        .read_buf	= xmp_read_buf,
        .write		= xmp_write,
        .write_buf	= xmp_write_buf,
        .statfs		= xmp_statfs,
        .flush		= xmp_flush,
        .release	= xmp_release,
        .fsync		= xmp_fsync,
#if defined(HAVE_POSIX_FALLOCATE) || defined(__APPLE__)
        .fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
        .setxattr	= xmp_setxattr,
        .getxattr	= xmp_getxattr,
        .listxattr	= xmp_listxattr,
        .removexattr	= xmp_removexattr,
#endif
#ifndef __APPLE__LOCK
        .lock		= xmp_lock,
#endif
#ifndef __APPLE__LOCK
        .flock		= xmp_flock,
#endif
#ifdef __APPLE__
        .setvolname	= xmp_setvolname,
        .exchange	= xmp_exchange,
        .getxtimes	= xmp_getxtimes,
        .setbkuptime	= xmp_setbkuptime,
        .setchgtime	= xmp_setchgtime,
        .setcrtime	= xmp_setcrtime,
        .chflags	= xmp_chflags,
        .setattr_x	= xmp_setattr_x,
        .fsetattr_x	= xmp_fsetattr_x,
#endif

        .flag_nullpath_ok = 1,
#if HAVE_UTIMENSAT
        .flag_utime_omit_ok = 1,
#endif
};

int main(int argc, char *argv[])
{
        umask(0);
        return fuse_main(argc, argv, &xmp_oper, NULL);
}
