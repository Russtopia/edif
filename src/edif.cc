/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2013  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "Macro.hh"

#include<iostream>
#include<fstream>
#include<string>

#include "Native_interface.hh"


#include "edif2.hh"
#include "gitversion.h"

#ifdef HAVE_CONFIG_H
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION
#include "../config.h"
#endif

#define APL_SUFFIX ".apl"
#define LAMBDA_PREFIX "_lambda_"

#define EDIF_DEFAULT "vi"
static char *edif_default = NULL;

using namespace std;
static bool is_lambda = false;

static char *dir = NULL;
static const Function *function = NULL;

class NativeFunction;

extern "C" void * get_function_mux(const char * function_name);
#if 0
static Token eval_ident_Bx(Value_P B, Axis x, const NativeFunction * caller);
static Token eval_fill_B(Value_P B, const NativeFunction * caller);
static Token eval_fill_AB(Value_P A, Value_P B, const NativeFunction * caller);
#endif

static bool
close_fun (Cause cause, const NativeFunction * caller)
{
  if (dir) {
    DIR *path;
    struct dirent *ent;
    if ((path = opendir (dir)) != NULL) {
      while ((ent = readdir (path)) != NULL) {
	char *lfn;
	asprintf (&lfn, "%s/%s", dir, ent->d_name);
	unlink (lfn);
	free (lfn);
      }
      closedir (path);
    } 

    rmdir (dir);
    free (dir);
    dir = NULL;
  }
  if (edif_default) {
    free (edif_default);
    edif_default = NULL;
  }
  return true;
}

Fun_signature
get_signature()
{
  asprintf (&dir, "/var/run/user/%d/%d",
	    (int)getuid (), (int)getpid ());
  mkdir (dir, 0700);
  char *ed = getenv ("EDIF");
  if (edif_default) free (edif_default);
  edif_default = strdup (ed ?: EDIF_DEFAULT);

  return SIG_Z_A_F2_B;
}

// export EDIF="vi"

static void
get_fcn (const char *fn, const char *base, Value_P B)
{
  UCS_string symbol_name(*B.get());
  while (symbol_name.back() <= ' ')   symbol_name.pop_back();
  if (symbol_name.size() != 0) {
    if (symbol_name[0] == UNI_MUE) {   // macro
      loop (m, Macro::MAC_COUNT) {
	const Macro * macro =
	  Macro::get_macro(static_cast<Macro::Macro_num>(m));
	if (symbol_name == macro->get_name()) {
	  function = macro;
	  break;
	}
      }
    }
    else {  // maybe user defined function
      NamedObject * obj = Workspace::lookup_existing_name(symbol_name);
      if (obj && obj->is_user_defined()) {
	function = obj->get_function();
	if (function && function->get_exec_properties()[0]) function = 0;
      }
    }
  }
  if (function != 0) {
    is_lambda |= function->is_lambda();
    const UCS_string ucs = function->canonical(false);
    UCS_string_vector tlines;
    ucs.to_vector(tlines);
    ofstream tfile;
    tfile.open (fn, ios::out);
    loop(row, tlines.size()) {
      const UCS_string & line = tlines[row];
      UTF8_string utf (line);
      if (is_lambda) {
	if (row == 0) continue;			// skip header
	else utf = UCS_string (utf, 2, -1);	// skip assignment
      }
      tfile << utf << endl;
    }
    tfile.close ();
  }
  else {
    ofstream tfile;
    tfile.open (fn, ios::out);
    if (!is_lambda) tfile << base << endl;
    tfile.close ();
  }
}

static void
cleanup (char *dir, UTF8_string base_name, char *fn)
{
  DIR *path;
  struct dirent *ent;
  if ((path = opendir (dir)) != NULL) {
    while ((ent = readdir (path)) != NULL) {
      if (!strncmp (ent->d_name, base_name.c_str (),
		    base_name.size ())) {
	char *lfn;
	asprintf (&lfn, "%s/%s", dir, ent->d_name);
	unlink (lfn);
	free (lfn);
      }
    }
    closedir (path);
  } 
  if (fn) free (fn);
}

static Token
eval_EB (const char *edif, Value_P B)
{
  function = NULL;
  if (B->is_char_string ()) {
    const UCS_string  ustr = B->get_UCS_ravel();
    UTF8_string base_name(ustr);

    char *fn = NULL;
    asprintf (&fn, "%s/%s.apl", dir, base_name.c_str ());

    APL_Integer nc = Quad_NC::get_NC(ustr);
    switch (nc) {
    case NC_FUNCTION:
    case NC_UNUSED_USER_NAME:
      {
	get_fcn (fn, base_name.c_str (), B);
	char *buf;
	asprintf (&buf, "%s %s", edif, fn);
	system (buf);
	if (buf) free (buf);

	ifstream tfile;
	tfile.open (fn, ios::in);
	UCS_string ucs;
	UCS_string lambda_ucs;
	if (tfile.is_open ()) {
	  string line;
	  int cnt = 0;
	  while (getline (tfile, line)) {
	    ucs.append_UTF8 (line.c_str ());
	    ucs.append(UNI_ASCII_LF);
	    if (cnt++ == 0) lambda_ucs.append_UTF8 (line.c_str ());
	  }
	  tfile.close ();
	  if (is_lambda) {
	    if (!lambda_ucs.empty ()) {
	      NamedObject * obj =
		Workspace::lookup_existing_name (UCS_string (base_name));
	      if (obj) {
		UCS_string erase_cmd(")ERASE ");
		erase_cmd.append (base_name);
		Bif_F1_EXECUTE::execute_command(erase_cmd);
	      }

	      UCS_string doit;
	      doit << base_name << "←{" << lambda_ucs << "}";
	      Command::do_APL_expression (doit);
	    }
	  }
	  else {
	    if (!ucs.empty ()) {
	      int error_line = 0;
	      UCS_string creator (base_name);
	      UTF8_string creator_utf8(creator);
	      UserFunction::fix (ucs,		// text
				 error_line,	// err_line
				 false,		// keep_existing
				 LOC,		// loc
				 creator_utf8,	// creator
				 true);		// tolerant
	    }
	  }
	}
	else {
	  UCS_string ucs ("Error opening working file.");
	  Value_P Z (ucs, LOC);
	  Z->check_value (LOC);
	  is_lambda = false;
	  return Token (TOK_APL_VALUE1, Z);
	}
	cleanup (dir, base_name, fn);
      }
      break;
    case NC_VARIABLE:
      {
	UCS_string ucs ("Variable editing not yet implemented.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	is_lambda = false;
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    default:
      {
	UCS_string ucs ("Unknown editing type requested.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	is_lambda = false;
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    }

    is_lambda = false;
    return Token(TOK_APL_VALUE1, Str0_0 (LOC));	// in case nothing works
  }
  else {
    UCS_string ucs ("Character string argument required.");
    Value_P Z (ucs, LOC);
    Z->check_value (LOC);
    return Token (TOK_APL_VALUE1, Z);
  }
}


static Token
eval_B (Value_P B)
{
  return eval_EB (edif_default, B);
}

static Token
eval_AB (Value_P A, Value_P B)
{
  if (A->is_char_string ()) {
    const UCS_string  ustr = A->get_UCS_ravel();
    UTF8_string edif (ustr);
    if (edif.c_str () && *(edif.c_str ())) {
      if (edif_default) free (edif_default);
      edif_default = strdup (edif.c_str ());
      return eval_EB (edif_default, B);
    }
    else {
      UCS_string ucs ("Invalid editor specification.");
      Value_P Z (ucs, LOC);
      Z->check_value (LOC);
      return Token (TOK_APL_VALUE1, Z);
    }
  }
  else {
    UCS_string ucs ("The editor specification must be a string.");
    Value_P Z (ucs, LOC);
    Z->check_value (LOC);
    return Token (TOK_APL_VALUE1, Z);
  }
}

#if 0
Token
eval_fill_B(Value_P B, const NativeFunction * caller)
{
UCS_string ucs("eval_fill_B() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

Token
eval_fill_AB(Value_P A, Value_P B, const NativeFunction * caller)
{
UCS_string ucs("eval_fill_B() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

Token
eval_ident_Bx(Value_P B, Axis x, const NativeFunction * caller)
{
UCS_string ucs("eval_ident_Bx() called");
Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
#endif

static Token
eval_XB(Value_P X, Value_P B)
{
  is_lambda = false;
  if (X->is_numeric_scalar()) {
    APL_Integer val = X->get_sole_integer();
    switch(val) {
    case 1: is_lambda = true; break;
    case 2:
      {
	Value_P vers (UCS_string(PACKAGE_STRING), LOC);
	return Token(TOK_APL_VALUE1, vers);
      }
      break;
    case 3: 
      {
	Value_P vers (UCS_string(GIT_VERSION), LOC);
	return Token(TOK_APL_VALUE1, vers);
      }
      break;
    }
  }
  return eval_EB (edif_default, B);
}


static Token
eval_AXB(Value_P A, Value_P X, Value_P B)
{
  is_lambda = false;
  if (X->is_numeric_scalar()) {
    APL_Integer val = X->get_sole_integer();
    switch(val) {
    case 1: is_lambda = true; break;
    case 2: 
      {
	Value_P vers (UCS_string(PACKAGE_STRING), LOC);
	return Token(TOK_APL_VALUE1, vers);
      }
      break;
    case 3: 
      {
	Value_P vers (UCS_string(GIT_VERSION), LOC);
	return Token(TOK_APL_VALUE1, vers);
      }
      break;
    }
  }
  return eval_AB (A, B);
}

void *
get_function_mux (const char * function_name)
{
   if (!strcmp (function_name, "get_signature")) return (void *)&get_signature;
   if (!strcmp (function_name, "eval_B"))        return (void *)&eval_B;
   if (!strcmp (function_name, "eval_XB"))       return (void *)&eval_XB;
   if (!strcmp (function_name, "eval_AB"))       return (void *)&eval_AB;
   if (!strcmp (function_name, "eval_AXB"))      return (void *)&eval_AXB;
#if 0
   if (!strcmp (function_name, "eval_fill_B"))   return (void *)&eval_fill_B;
   if (!strcmp (function_name, "eval_fill_AB"))  return (void *)&eval_fill_AB;
   if (!strcmp (function_name, "eval_ident_Bx")) return (void *)&eval_ident_Bx;
#endif
   if (!strcmp (function_name, "close_fun"))
     return reinterpret_cast<void *>(&close_fun);
   return 0;
}
