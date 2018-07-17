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

#include "Macro.hh"

#include<iostream>
#include<fstream>
#include<string>

#if 0
#include "native/template.hh"
#else
#include "Value.icc"
#include "Native_interface.hh"

class NativeFunction;

extern "C" void * get_function_mux(const char * function_name);
static Token eval_ident_Bx(Value_P B, Axis x, const NativeFunction * caller);
static Token eval_fill_B(Value_P B, const NativeFunction * caller);
static Token eval_fill_AB(Value_P A, Value_P B, const NativeFunction * caller);

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

#define EDIF_DEFAULT \
  "emacs --geometry=40x20 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

using namespace std;

char *dir = NULL;

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
  return true;
}

Fun_signature
get_signature()
{
  asprintf (&dir, "/var/run/user/%d/%d",
	    (int)getuid (), (int)getpid ());
  mkdir (dir, 0700);
  cerr << "opening " << dir << endl;

  return SIG_Z_A_F2_B;
}

// export EDIF="emacs --geometry=80x60 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

// apl --LX "'./.libs/libedif.so' ⎕fx 'edif'"

static void
get_fcn (const char *fn, const char *base, Value_P B)
{
  UCS_string symbol_name(*B.get());
  while (symbol_name.back() <= ' ')   symbol_name.pop_back();
  const Function * function = 0;
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
    const UCS_string ucs = function->canonical(false);
    UCS_string_vector tlines;
    ucs.to_vector(tlines);
    ofstream tfile;
    tfile.open (fn, ios::out);
    loop(row, tlines.size()) {
      const UCS_string & line = tlines[row];
      UTF8_string utf (line);
      tfile << utf << endl;
    }
    tfile.close ();
  }
  else {
    ofstream tfile;
    tfile.open (fn, ios::out);
    tfile << base << endl;
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

    /***
	from NamedObject.hh:

NC_INVALID          = -1,   ///< invalid name class.
NC_UNUSED_USER_NAME =  0,   ///< unused user name eg. pushed but not assigned
NC_LABEL            =  1,   ///< Label.
NC_VARIABLE         =  2,   ///< (assigned) variable.
NC_FUNCTION         =  3,   ///< (user defined) function.
NC_OPERATOR         =  4,   ///< (user defined) operator.
NC_SHARED_VAR       =  5,   ///< shared variable.

     ***/

static Token
eval_EB (const char *edif, Value_P B)
{
  if (B->is_char_string ()) {
    const UCS_string  ustr = B->get_UCS_ravel();
    UTF8_string base_name(ustr);

#if 0
#endif
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
	if (tfile.is_open ()) {
	  string line;
	  while (getline (tfile, line)) {
	    ucs.append_utf8 (line.c_str ());
	    ucs.append(UNI_ASCII_LF);
	  }
	  tfile.close ();
	  int error_line = 0;
	  UCS_string creator (base_name);
	  creator.append (UNI_ASCII_COLON);
	  creator.append_number (0);
	  UTF8_string creator_utf8(creator);
	  UserFunction::fix (ucs,		// text
			     error_line,	// err_line
			     false,		// keep_existing
			     LOC,		// loc
			     creator_utf8,	// creator
			     true);		// tolerant
	  cleanup (dir, base_name, fn);
	}
	else {
	  UCS_string ucs ("Error opening working file.");
	  Value_P Z (ucs, LOC);
	  Z->check_value (LOC);
	  return Token (TOK_APL_VALUE1, Z);
	}
      }
      break;
    case NC_VARIABLE:
      {
	UCS_string ucs ("Variable editing not yet implemented.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    default:
      {
	UCS_string ucs ("Unknown editing type requested.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    }

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
  static char *edif;
  if (!edif) edif = getenv ("EDIF");
  if (!edif) edif = strdup (EDIF_DEFAULT);
  return eval_EB (edif, B);
}

static Token
eval_AB (Value_P A, Value_P B)
{
  if (A->is_char_string ()) {
    const UCS_string  ustr = A->get_UCS_ravel();
    UTF8_string edif (ustr);
    if (edif.c_str () && *(edif.c_str ()))
      return eval_EB (edif.c_str (), B);
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
static Token
eval_XB (Value_P A, Value_P B, const NativeFunction * caller)
{
  cerr << "in eval_XB()\n";
  return Token (TOK_APL_VALUE1, Str0_0 (LOC));
}

static Token
eval_AXB (Value_P A, Value_P X, Value_P B)
{
  cerr << "in eval_AXB()\n";
  return Token (TOK_APL_VALUE1, Str0_0 (LOC));
}
#endif
  
void *
get_function_mux (const char * function_name)
{
   if (!strcmp (function_name, "get_signature")) return (void *)&get_signature;
   if (!strcmp (function_name, "eval_B"))        return (void *)&eval_B;
   if (!strcmp (function_name, "eval_AB"))       return (void *)&eval_AB;
#if 0
   if (!strcmp (function_name, "eval_XB"))       return (void *)&eval_XB;
   if (!strcmp (function_name, "eval_AXB"))      return (void *)&eval_AXB;
#endif
   if (!strcmp (function_name, "eval_ident_Bx")) return (void *)&eval_ident_Bx;
   if (!strcmp (function_name, "eval_fill_B"))   return (void *)&eval_fill_B;
   if (!strcmp (function_name, "eval_fill_AB"))  return (void *)&eval_fill_AB;
   if (!strcmp (function_name, "close_fun"))
     return reinterpret_cast<void *>(&close_fun);
   return 0;
}
