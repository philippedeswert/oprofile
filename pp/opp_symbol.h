/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * first written by P.Elie.
 */
#ifndef OPP_SYMBOL_H
#define OPP_SYMBOL_H

#include <string>
#include <vector>
#include <iostream>

#include <bfd.h>	/* for bfd_vma */

#include "oprofpp.h"

class samples_files_t;
class field_description;

//---------------------------------------------------------------------------
/// A simple container for a fileno:linr location
struct file_location {
	/// From where image come this file location
	std::string image_name;
	/// empty if not valid.
	std::string filename;
	/// 0 means invalid or code is generated internally by the compiler
	int linenr;

	bool operator <(const file_location & rhs) const {
		return filename < rhs.filename ||
			(filename == rhs.filename && linenr < rhs.linenr);
	}
};

//---------------------------------------------------------------------------
/// associate vma address with a file location and a samples count
struct sample_entry {
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	counter_array_t counter;
};

//---------------------------------------------------------------------------
/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	std::string name;
	/// [first, last[ gives the range of sample_entry.
	size_t first;
	size_t last;
};

//---------------------------------------------------------------------------
/// flags passed to the ctr of an OutputSymbol object. This also specify the
/// order of field output: lower enum tag ==> comes first in output order
/// Note than changing value of enum is not safe. See OutputSymbol and
/// the osf_repeat_mask use. So you can't reorder easily the output order
/// of field...
enum OutSymbFlag {
	osf_none = 0,
	osf_vma = 1 << 0,
	/// this four field can be repeated on output for each counter.
	osf_nr_samples = 1 << 1,
	osf_nr_samples_cumulated = 1 << 2,
	osf_percent = 1 << 3,
	osf_percent_cumulated = 1 << 4,
	// used internally
	osf_repeat_mask = osf_nr_samples + osf_nr_samples_cumulated +
			  osf_percent + osf_percent_cumulated,
//	osf_time = 1 << 5,		// can be usefull, future. Do not use
//	osf_time_cumulated = 1 << 6,	// for now. FIXME is this usefull ?
	osf_symb_name = 1 << 7,
	// TODO: actually demangling is made when storing symb name in
	// samples_files_t. We need perhaps to store raw name in symbol and
	// defer demangling at output time.
//	osf_demangle = 1 << 8,		// provide demangle param name etc ?
	osf_linenr_info = 1 << 9,
	osf_short_linenr_info = 1 << 10,// w/o path name
	osf_image_name = 1 << 11,
	osf_short_image_name = 1 << 12,	// w/o path name
	osf_details = 1 << 13,		// for oprofpp -L / -s
	// only this field are showed in symbols samples details.
	osf_details_mask = osf_nr_samples + osf_nr_samples_cumulated +
			osf_percent + osf_percent_cumulated +
			osf_vma + osf_linenr_info + osf_short_linenr_info,
	osf_show_all_counters = 1 << 14,// for oprofpp -L
	// special format modifier: output all field selected separated by a
	// single space. This is intended to run post profile tools as
	// front-end of other tools which treat samples information.
//	osf_raw_format = 1 << 15,	// future
	osf_no_header = 1 << 16
};

class OutputSymbol {
public:
	/// build an OutputSymbol object, the samples_files_t life time object
	/// must be > of the life time of the OutputSymbol object.
	OutputSymbol(const samples_files_t & samples_files, int counter);

	/// convenience flag to set one flags w/o worrying about cast
	void SetFlag(OutSymbFlag flag);

	void Output(std::ostream & out, const symbol_entry * symb);
	void Output(std::ostream & out,
		    const std::vector<const symbol_entry *> & v, bool reverse);

	static void ShowHelp();

	/// the set of formating function, used internally by Output(), exposed
	/// as public member for the future oprofpp GUI.
	std::string format_vma(const std::string & name,
			       const sample_entry & sample, int);
	std::string format_symb_name(const std::string & name,
				     const sample_entry & sample, int);
	std::string format_image_name(const std::string & name,
				      const sample_entry & sample, int);
	std::string format_short_image_name(const std::string & name,
					    const sample_entry & sample, int);
	std::string format_linenr_info(const std::string & name,
				       const sample_entry & sample, int);
	std::string format_short_linenr_info(const std::string & name,
					     const sample_entry & sample, int);
	std::string format_nr_samples(const std::string & name,
				      const sample_entry & sample, int ctr);
	std::string format_nr_cumulated_samples(const std::string & name,
					const sample_entry & sample, int);
	std::string format_percent(const std::string & name,
				   const sample_entry & sample, int);
	std::string format_cumulated_percent(const std::string & name,
					     const sample_entry & sample, int);
private:
	void DoOutput(std::ostream & out, const std::string & name,
		      const sample_entry & sample, OutSymbFlag flags);
	void OutputDetails(std::ostream & out, const symbol_entry * symb);
	void OutputHeader(std::ostream & out);
	void OutputField(std::ostream & out, const std::string & name,
			 const sample_entry & sample,
			 OutSymbFlag fl, int ctr);
	void OutputHeaderField(std::ostream & out, OutSymbFlag fl);
	static const field_description * GetFieldDescr(OutSymbFlag flag);

	OutSymbFlag flags;
	const samples_files_t & samples_files;
	u32 total_count[OP_MAX_COUNTERS];
	u32 cumulated_samples[OP_MAX_COUNTERS];
	u32 cumulated_percent[OP_MAX_COUNTERS];
	// probably not an u32 ...
//	u32 time_cumulated[OP_MAX_COUNTERS];
	int counter;
	bool first_output;
};

/// return osf_none if the option string is ill formed
OutSymbFlag ParseOutputOption(const std::string & option);

// backward compatible with old stuff. Remove when OutputSymbol work proprely.
void output_symbol(const symbol_entry* symb, bool show_image_name,
		   bool output_linenr_info, int counter, u32 total_count);

#endif /* !OPP_SYMBOL_H */