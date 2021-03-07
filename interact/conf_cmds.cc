/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <config.h>
#include <lispCli.h>
#include "all_cmds.h"


static int process_conf_set_int (int argc, char **argv)
{
  int v;
  if (argc != 3) {
    fprintf (stderr, "Usage: %s <name> <val>\n", argv[0]);
    return 0;
  }
  v = atoi (argv[1]);
  config_set_int (argv[1], v);
  return 1;
}

static int process_conf_get_int (int argc, char **argv)
{
  int v;
  if (argc != 2) {
    fprintf (stderr, "Usage: %s <name>\n", argv[0]);
    return 0;
  }
  v = config_get_int (argv[1]);
  LispSetReturnInt (v);
  return 2;
}

static int process_conf_set_string (int argc, char **argv)
{
  if (argc != 3) {
    fprintf (stderr, "Usage: %s <name> <val>\n", argv[0]);
    return 0;
  }
  config_set_string (argv[1], argv[2]);
  return 1;
}

static int process_conf_get_string (int argc, char **argv)
{
  if (argc != 2) {
    fprintf (stderr, "Usage: %s <name>\n", argv[0]);
    return 0;
  }
  LispSetReturnString (config_get_string (argv[1]));
  return 3;
}

static int process_conf_set_real (int argc, char **argv)
{
  double v;
  if (argc != 3) {
    fprintf (stderr, "Usage: %s <name> <val>\n", argv[0]);
    return 0;
  }
  v = atof (argv[1]);
  config_set_real (argv[1], v);
  return 1;
}

static int process_conf_get_real (int argc, char **argv)
{
  double v;
  if (argc != 2) {
    fprintf (stderr, "Usage: %s <name>\n", argv[0]);
    return 0;
  }
  LispSetReturnFloat (config_get_real (argv[1]));
  return 4;
}

static struct LispCliCommand conf_cmds[] = {
  { NULL, "Configuration parameters (use `act:conf:' prefix)", NULL },
  { "set_int", "set_int <name> <val> - set integer config parameter", process_conf_set_int },
  { "set_string", "set_string <name> <val> - set string config parameter", process_conf_set_string },
  { "set_real", "set_real <name> <val> - set real config parameter", process_conf_set_real },
  { "get_int", "set_int <name> - return integer config parameter", process_conf_get_int },
  { "get_string", "get_string <name> - get string config parameter", process_conf_get_string },
  { "get_real", "get_real <name> - get real config parameter", process_conf_get_real }
};


void conf_cmds_init (void)
{
  LispCliAddCommands ("act:conf", conf_cmds, sizeof (conf_cmds)/sizeof (conf_cmds[0]));
}
