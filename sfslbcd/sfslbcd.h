/*
 *
 * Copyright (C) 2002 Benjie Chen (benjie@lcs.mit.edu)
 * Copyright (C) 1998 David Mazieres (dm@uun.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include "arpc.h"
#include "sfscd_prot.h"
#include "nfstrans.h"
#include "sfsclient.h"
#include "qhash.h"
#include "itree.h"
#include "crypt.h"
#include "list.h"

inline
const strbuf &
strbuf_cat(const strbuf &b, nfs_fh3 fh)
{
  str s = armor32(fh.data.base(), fh.data.size());
  b << s;
  return b;
}

class attr_cache {
  struct access_dat {
    u_int32_t mask;
    u_int32_t perm;

    access_dat (u_int32_t m, u_int32_t p) : mask (m), perm (p) {}
  };

public:
  struct attr_dat {
    attr_cache *const cache;
    const nfs_fh3 fh;
    ex_fattr3 attr;
    qhash<sfs_aid, access_dat> access;

    ihash_entry<attr_dat> fhlink;
    tailq_entry<attr_dat> lrulink;

    attr_dat (attr_cache *c, const nfs_fh3 &f, const ex_fattr3 *a);
    ~attr_dat ();
    void touch ();
    void set (const ex_fattr3 *a, const wcc_attr *w);
    bool valid () { return timenow < implicit_cast<time_t> (attr.expire); }
  };

private:
  friend class attr_dat;
  ihash<const nfs_fh3, attr_dat, &attr_dat::fh, &attr_dat::fhlink> attrs;

  static void remove_aid (sfs_aid aid, attr_dat *ad)
    { ad->access.remove (aid); }

public:
  ~attr_cache () { attrs.deleteall (); }
  void flush_attr () { attrs.deleteall (); }
  void flush_access (sfs_aid aid) { attrs.traverse (wrap (remove_aid, aid)); }
  void flush_access (const nfs_fh3 &fh, sfs_aid);

  void attr_enter (const nfs_fh3 &, const ex_fattr3 *, const wcc_attr *);
  const ex_fattr3 *attr_lookup (const nfs_fh3 &);

  void access_enter (const nfs_fh3 &, sfs_aid aid,
		     u_int32_t mask, u_int32_t perm);
  int32_t access_lookup (const nfs_fh3 &, sfs_aid, u_int32_t mask);
};

class server : public sfsserver_auth {
  attr_cache ac;

  void dispatch_dummy (svccb *sbp);
  void cbdispatch (svccb *sbp);
  void getreply (time_t rqtime, nfscall *nc, void *res, clnt_stat err);
  void setfd(int fd);

  void cache_file(time_t rqtime, nfscall *nc, void *res, clnt_stat err);
  void cache_file_reply(time_t rqtime, nfscall *nc, void *res, bool ok);

public:
  typedef sfsserver_auth super;
  ptr<aclnt> nfsc;
  ptr<asrv> nfscbs;

  server (const sfsserverargs &a) : sfsserver_auth (a) {}
  ~server () { warn << path << " deleted\n"; }
  void flushstate ();
  void authclear (sfs_aid aid);
  void setrootfh (const sfs_fsinfo *fsi, callback<void, bool>::ref err_c);
  void dispatch (nfscall *nc);
};

void lbfs_read(nfs_fh3 fh, size_t size, ref<aclnt> c, AUTH *a,
               callback<void, bool>::ref cb);

