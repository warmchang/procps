/*
 * select.c - ps process selection
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2025 Craig Small <csmall@dropbear.xyz
 * Copyright © 1998-2002 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"

#define running(p)              (rSv(STATE, s_ch, p) == 'R' || rSv(STATE, s_ch, p) == 'D')

/* process selection flags */
static bool select_my_euid = false;
static bool select_my_tty = false;
static bool select_running = false;
static bool select_no_leader = false;
static bool select_has_tty = false;

const char *select_setup(void)
{
    /* SunOS by default will not select session leaders without the g option */
    if ( (personality & PER_NO_DEFAULT_g) && !(simple_select & (SS_B_g)))
        select_no_leader = true;

    switch (simple_select) {
        case 0:
            /* The default with no selection is my euid */
            select_my_euid = true;
            /* BSD wants any tty, non-BSD wants my TTY */
            if (prefer_bsd_defaults == 1)
                select_has_tty = true;
            else
                select_my_tty = true;
            break;
        case SS_U_a:
            /* -a flag: Not session leaders, has a tty */
            select_no_leader = true;
            select_has_tty = true;
            break;
        case SS_U_d:
            /* fallthrough - they are the same */
        case SS_U_a | SS_U_d:
            /* -d or -da flags: Not session leaders */
            select_no_leader = true;
            break;
        /* BSD style options */
        case SS_B_a:
        case SS_B_a | SS_B_g:
            select_has_tty = true;
            break;
        case SS_B_x:
        case SS_B_x | SS_B_g:
            select_my_euid = true;
            break;
        case SS_B_a | SS_B_x:
        case SS_B_a | SS_B_x | SS_B_g:
            /* everything */
            simple_select = 0;
            all_processes = 1;
            break;
        default:
            return _("process selection options conflict");
    }
    return NULL; /* no error */
}

static bool simple_accept(
    proc_t *p)
{
    if (select_my_euid && rSv(ID_EUID, u_int, p) != cached_euid)
        return false;
    if (select_my_tty && rSv(TTY, s_int, p) != cached_tty)
        return false;
    if (select_running && (rSv(STATE, s_ch, p) != 'R' && rSv(STATE, s_ch, p) != 'D'))
        return false;
    if (select_no_leader && (rSv(ID_SESSION, s_int, p) == rSv(ID_TGID, s_int, p)))
        return false;
    if (select_has_tty && (rSv(TTY, s_int, p)== 0))
        return false;
    return true;
}

/***** selected by some kind of list? */
static int proc_was_listed(proc_t *buf){
  selection_node *sn = selection_list;
  int i;
  if(!sn) return 0;
  while(sn){
    switch(sn->typecode){
    default:
      catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));

#define return_if_match(foo,bar) \
        i=sn->n; while(i--) \
        if((unsigned)foo == (unsigned)(*(sn->u+i)).bar) \
        return 1

    break; case SEL_RUID: return_if_match(rSv(ID_RUID, u_int, buf),uid);
    break; case SEL_EUID: return_if_match(rSv(ID_EUID, u_int, buf),uid);
    break; case SEL_SUID: return_if_match(rSv(ID_SUID, u_int, buf),uid);
    break; case SEL_FUID: return_if_match(rSv(ID_FUID, u_int, buf),uid);

    break; case SEL_RGID: return_if_match(rSv(ID_RGID, u_int, buf),gid);
    break; case SEL_EGID: return_if_match(rSv(ID_EGID, u_int, buf),gid);
    break; case SEL_SGID: return_if_match(rSv(ID_SGID, u_int, buf),gid);
    break; case SEL_FGID: return_if_match(rSv(ID_FGID, u_int, buf),gid);

    break; case SEL_PGRP: return_if_match(rSv(ID_PGRP, s_int, buf),pid);
    break; case SEL_PID : return_if_match(rSv(ID_TGID, s_int, buf),pid);
    break; case SEL_PID_QUICK : return_if_match(rSv(ID_TGID, s_int, buf),pid);
    break; case SEL_PPID: return_if_match(rSv(ID_PPID, s_int, buf),ppid);
    break; case SEL_TTY : return_if_match(rSv(TTY, s_int, buf),tty);
    break; case SEL_SESS: return_if_match(rSv(ID_SESSION, s_int, buf),pid);

    break;
    case SEL_COMM:
        i=sn->n;
        while(i--) {
            /* special case, comm is 16 characters but match is longer */
            if (strlen(rSv(CMD, str, buf)) == 15 && strlen((*(sn->u+i)).cmd) >= 15)
                if(!strncmp( rSv(CMD, str, buf), (*(sn->u+i)).cmd, 15 )) return 1;
            if(!strncmp( rSv(CMD, str, buf), (*(sn->u+i)).cmd, 63 )) return 1;
        }


#undef return_if_match

    }
    sn = sn->next;
  }
  return 0;
}


/***** This must satisfy Unix98 and as much BSD as possible */
int want_this_proc(proc_t *buf){
  int accepted_proc = 1; /* assume success */
  /* elsewhere, convert T to list, U sets x implicitly */

  /* handle -e -A */
  if(all_processes) goto finish;

  /* use table for -a a d g x */
  if((simple_select || !selection_list))
    if(simple_accept(buf)) goto finish;

  /* search lists */
  if(proc_was_listed(buf)) goto finish;

  /* fail, fall through to loose ends */
  accepted_proc = 0;

  /* do r N */
finish:
  if(running_only && !running(buf)) accepted_proc = 0;
  if(negate_selection) return !accepted_proc;
  return accepted_proc;
}
