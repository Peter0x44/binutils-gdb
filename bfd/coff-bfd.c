/* BFD COFF interfaces used outside of BFD.
   Copyright (C) 1990-2026 Free Software Foundation, Inc.
   Written by Cygnus Support.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "libcoff.h"

/*
FUNCTION
	bfd_coff_get_comdat_associative_parent

SYNOPSIS
	asection *bfd_coff_get_comdat_associative_parent
	  (asection *section);

DESCRIPTION
	Return the immediate parent of an associative PE/COFF COMDAT
	@var{section}, or <<NULL>> if @var{section} is not associative.
*/

asection *
bfd_coff_get_comdat_associative_parent (asection *section)
{
  struct coff_section_tdata *section_data;

  if (section == NULL || section->owner == NULL
      || bfd_get_flavour (section->owner) != bfd_target_coff_flavour
      || !obj_pe (section->owner))
    return NULL;

  section_data = coff_section_data (section->owner, section);
  if (!coff_section_data_associative (section_data))
    return NULL;

  return section_data->comdat_associated_parent;
}

/*
FUNCTION
	bfd_coff_set_comdat_associative

SYNOPSIS
	bool bfd_coff_set_comdat_associative
	  (asection *section, asection *parent);

DESCRIPTION
	Make the PE/COFF COMDAT @var{section} associative with @var{parent}.
	Both sections must belong to the same PE/COFF BFD, and @var{parent}
	must already be a link-once section.  Return <<TRUE>> on success.
*/

bool
bfd_coff_set_comdat_associative (asection *section, asection *parent)
{
  bfd *abfd;
  asection *ancestor;
  struct coff_section_tdata *ancestor_data;
  struct coff_section_tdata *parent_data;
  struct coff_section_tdata *section_data;
  unsigned int depth;
  bool already_associative;

  if (section == NULL || parent == NULL || section == parent
      || section->owner == NULL || section->owner != parent->owner
      || bfd_get_flavour (section->owner) != bfd_target_coff_flavour
      || !obj_pe (section->owner) || (parent->flags & SEC_LINK_ONCE) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  abfd = section->owner;
  if (coff_section_data (abfd, section) == NULL)
    {
      section->used_by_bfd = bfd_zalloc (abfd,
					 sizeof (struct coff_section_tdata));
      if (section->used_by_bfd == NULL)
	return false;
    }
  if (coff_section_data (abfd, parent) == NULL)
    {
      parent->used_by_bfd = bfd_zalloc (abfd,
					sizeof (struct coff_section_tdata));
      if (parent->used_by_bfd == NULL)
	return false;
    }

  section_data = coff_section_data (abfd, section);
  already_associative = coff_section_data_associative (section_data);
  if (already_associative
      && section_data->comdat_associated_parent != parent)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  /* Validate the new edge before recording it.  PE/COFF permits associative
     chains, but every chain must terminate at a non-associative COMDAT.  */
  ancestor = parent;
  depth = 0;
  while (ancestor != NULL)
    {
      if (ancestor == section || ++depth > abfd->section_count)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  return false;
	}

      ancestor_data = coff_section_data (abfd, ancestor);
      if (!coff_section_data_associative (ancestor_data))
	break;
      ancestor = ancestor_data->comdat_associated_parent;
    }

  section_data->comdat_selection = COFF_COMDAT_SELECT_ASSOCIATIVE;
  section_data->comdat_associated_parent_index = parent->target_index;
  section_data->comdat_associated_parent = parent;

  if (!already_associative)
    {
      parent_data = coff_section_data (abfd, parent);
      section_data->comdat_associated_next
	= parent_data->comdat_associated_child;
      parent_data->comdat_associated_child = section;
    }

  section->flags &= ~SEC_LINK_DUPLICATES;
  section->flags |= SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD;
  return true;
}

/* Return the COFF syment for a symbol.  */

bool
bfd_coff_get_syment (bfd *abfd,
		     asymbol *symbol,
		     struct internal_syment *psyment)
{
  coff_symbol_type *csym;

  csym = coff_symbol_from (symbol);
  if (csym == NULL || csym->native == NULL
      || ! csym->native->is_sym)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  *psyment = csym->native->u.syment;

  if (csym->native->fix_value)
    {
      psyment->n_value =
	((psyment->n_value - (uintptr_t) obj_raw_syments (abfd))
	 / sizeof (combined_entry_type));
      csym->native->fix_value = 0;
    }

  /* FIXME: We should handle fix_line here.  */

  return true;
}

/* Return the COFF auxent for a symbol.  */

bool
bfd_coff_get_auxent (bfd *abfd,
		     asymbol *symbol,
		     int indx,
		     union internal_auxent *pauxent)
{
  coff_symbol_type *csym;
  combined_entry_type *ent;

  csym = coff_symbol_from (symbol);

  if (csym == NULL
      || csym->native == NULL
      || ! csym->native->is_sym
      || indx >= csym->native->u.syment.n_numaux)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  ent = csym->native + indx + 1;

  BFD_ASSERT (! ent->is_sym);
  *pauxent = ent->u.auxent;

  if (ent->fix_tag)
    {
      pauxent->x_sym.x_tagndx.u32 =
	((combined_entry_type *) pauxent->x_sym.x_tagndx.p
	 - obj_raw_syments (abfd));
      ent->fix_tag = 0;
    }

  if (ent->fix_end)
    {
      pauxent->x_sym.x_fcnary.x_fcn.x_endndx.u32 =
	((combined_entry_type *) pauxent->x_sym.x_fcnary.x_fcn.x_endndx.p
	 - obj_raw_syments (abfd));
      ent->fix_end = 0;
    }

  if (ent->fix_scnlen)
    {
      pauxent->x_csect.x_scnlen.u64 =
	((combined_entry_type *) pauxent->x_csect.x_scnlen.p
	 - obj_raw_syments (abfd));
      ent->fix_scnlen = 0;
    }

  return true;
}

/* Return TRUE if SYMBOL is a PE weak external whose fallback symbol is
   a real definition.  A weak declaration with no fallback uses the COFF
   null symbol as its fallback; do not treat that as an archive provider.  */

bool
bfd_coff_pe_weak_external_has_real_fallback (bfd *abfd,
					    asymbol *symbol)
{
  coff_symbol_type *csym;
  combined_entry_type *aux;
  combined_entry_type *fallback;

  if (bfd_get_flavour (abfd) != bfd_target_coff_flavour
      || coff_data (abfd) == NULL
      || ! obj_pe (abfd))
    return false;

  csym = coff_symbol_from (symbol);
  if (csym == NULL
      || csym->native == NULL
      || ! csym->native->is_sym
      || csym->native->u.syment.n_sclass != C_NT_WEAK
      || csym->native->u.syment.n_numaux != 1)
    return false;

  aux = csym->native + 1;
  if (aux->is_sym)
    return false;

  if (aux->fix_tag)
    fallback = (combined_entry_type *) aux->u.auxent.x_sym.x_tagndx.p;
  else
    {
      uint32_t tagndx = aux->u.auxent.x_sym.x_tagndx.u32;

      if (tagndx >= obj_raw_syment_count (abfd))
	return false;
      fallback = obj_raw_syments (abfd) + tagndx;
    }

  return (fallback != NULL
	  && fallback->is_sym
	  && fallback->u.syment.n_scnum != N_UNDEF
	  && !(fallback->u.syment.n_scnum == N_ABS
	       && fallback->u.syment.n_value == 0));
}
