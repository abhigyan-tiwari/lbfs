// -*-c++-*-
/* $Id$ */

/*
 *
 * Copyright (C) 2001 David Mazieres (dm@uun.org)
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

#ifndef _AXPRT_COMPRESS_H_
#define _AXPRT_COMPRESS_H_ 1

#include "arpc.h"
#include "zlib.h"

class axprt_compress : public axprt {
protected:
  bool docompress;
  recvcb_t cb;
  ref<axprt> x;

  // Implement this -- uncompress and call (*cb)
  void rcb (const char *buf, ssize_t len, const sockaddr *sa);

public:
  axprt_compress (ref<axprt> xx)
    : axprt (true, true, xx->socksize), docompress (false), x (xx) {
    assert (x->reliable && x->connected);
    x->setrcb (NULL);
  }
  // Implement this -- compress and call x->sendv
  void sendv (const iovec *, int, const sockaddr *);
  void setwcb (cbv cb) { x->setwcb (cb); }
  void setrcb (recvcb_t c) {
    cb = c;
    if (cb)
      x->setrcb (wrap (this, &axprt_compress::rcb));
    else
      x->setrcb (NULL);
  }
  bool ateof () { return x->ateof (); }
  void compress () { docompress = true; }
  
  static ref<axprt_compress> alloc (ref<axprt> xx)
    { return New refcounted<axprt_compress> (xx); }
};

#endif /* _AXPRT_COMPRESS_H_ */
