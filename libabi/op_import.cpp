/**
 * @file op_import.cpp
 * Import sample files from other ABI
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Graydon Hoare 
 */

#include "abi.h"
#include "odb_hash.h"
#include "popt_options.h"
#include "op_sample_file.h"

#include <fstream>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using namespace std;

namespace {
	string output_filename;
	string abi_filename;
	bool verbose;
	bool force;
};

popt::option options_array[] = {
	popt::option(verbose, "verbose", 'V', "verbose output"),
	popt::option(output_filename, "output", 'o', "output to file", "filename"),
	popt::option(abi_filename, "abi", 'a', "abi description", "filename"),
	popt::option(force, "force", 'f', "force conversion, even if identical")
};
 
struct Extractor 
{
	Abi const & abi;

	unsigned char const * begin;
	unsigned char const * end;
	bool little_endian;

	explicit Extractor(Abi const & a, unsigned char const * src, size_t len) : 
		abi(a), 
		begin(src),
		end(src + len) 
	{
		little_endian = abi.need(string("little_endian")) == 1;
		if (verbose) {
			cerr << "source byte order is: " 
			     << string(little_endian ? "little" : "big")
			     << " endian" << endl;
		}
	}

	template <typename T>
	void extract(T & targ, void const * src_, const char * sz, const char * off);
};



template <typename T>
void Extractor::extract(T & targ, void const * src_, const char * sz, const char * off)
{
	unsigned char const * src = static_cast<unsigned char const *>(src_) + abi.need(off);
	size_t nbytes = abi.need(sz);
	
	if (nbytes == 0) 
		return;
	
	assert(nbytes <= sizeof(T));
	assert(src >= begin);
	assert(src + nbytes <= end);
	
	if (verbose)
		cerr << hex << "get " << sz << " = " << nbytes 
		     << " bytes @ " << off << " = " << (src - begin)
		     << " : ";

	targ = 0;
	if (little_endian)
		while(nbytes--)
			targ = (targ << 8) | src[nbytes];
	else
		for(size_t i = 0; i < nbytes; ++i)
			targ = (targ << 8) | src[i];
	
	if (verbose)
		cerr << " = " << targ << endl;
}



void import_from_abi(Abi const & abi, 
		     void const * srcv, 
		     size_t len, 
		     samples_odb_t * dest) throw (Abi_exception)
{
	struct opd_header * head;
	head = static_cast<opd_header *>(dest->base_memory);	
	unsigned char const * src = static_cast<unsigned char const *>(srcv);
	unsigned char const * const begin = src;
	Extractor ext(abi, src, len);	

	memcpy(head->magic, src + abi.need("offsetof_header_magic"), 4);

	// begin extracting opd header
	ext.extract(head->version, src, "sizeof_u32", "offsetof_header_version");
	ext.extract(head->is_kernel, src, "sizeof_u8", "offsetof_header_is_kernel");
	ext.extract(head->ctr_event, src, "sizeof_u32", "offsetof_header_ctr_event");
	ext.extract(head->ctr_um, src, "sizeof_u32", "offsetof_header_ctr_um");
	ext.extract(head->ctr, src, "sizeof_u32", "offsetof_header_ctr");
	ext.extract(head->cpu_type, src, "sizeof_u32", "offsetof_header_cpu_type");
	ext.extract(head->ctr_count, src, "sizeof_u32", "offsetof_header_ctr_count");
	head->cpu_speed = 0.; // "double" extraction is unlikely to work
	ext.extract(head->mtime, src, "sizeof_time_t", "offsetof_header_mtime");
	ext.extract(head->separate_lib_samples, src, "sizeof_int", "offsetof_header_separate_lib_samples");
	ext.extract(head->separate_kernel_samples, src, "sizeof_int", "offsetof_header_separate_kernel_samples");
	src += abi.need("sizeof_struct_opd_header");
	// done extracting opd header

	// begin extracting necessary parts of descr
	odb_node_nr_t node_nr;
	ext.extract(node_nr, src, "sizeof_odb_node_nr_t", "offsetof_descr_current_size");
	src += abi.need("sizeof_odb_descr_t");
	// done extracting descr

	// skip node zero, it is reserved and contains nothing usefull
	src += abi.need("sizeof_odb_node_t");

	// begin extracting nodes
	unsigned int step = abi.need("sizeof_odb_node_t");
	if (verbose)
		cerr << "extracting " << node_nr << " nodes of " << step << " bytes each " << endl;

	assert(src + (node_nr * step) <= begin + len);

	for (odb_node_nr_t i = 1 ; i < node_nr ; ++i, src += step) {
		odb_key_t key;
		odb_value_t val;
		ext.extract(key, src, "sizeof_odb_key_t", "offsetof_node_key");
		ext.extract(val, src, "sizeof_odb_value_t", "offsetof_node_value");
		char * err_msg;
		int rc = odb_insert(dest, key, val, &err_msg);
		if (rc != EXIT_SUCCESS) {
			cerr << err_msg << endl;
			free(err_msg);
			exit(EXIT_FAILURE);
		}
	}
	// done extracting nodes
}



int 
main(int argc, char const ** argv) 
{

	vector<string> inputs;
	popt::parse_options(argc, argv, inputs);

	if (inputs.size() != 1) {
		cerr << "error: must specify exactly 1 input file" << endl;
		exit(1);
	}

	Abi current_abi, input_abi;
	{
		ifstream abi_file(abi_filename.c_str());
		if (!abi_file) {
			cerr << "error: cannot open abi file " 
			     << abi_filename << endl;
			exit(1);
		}
		abi_file >> input_abi;
	}

	if (!force && current_abi == input_abi) {
		cerr << "input abi is identical to native. "
		     << "no conversion necessary." << endl;
		exit(1);
	} else {
		int in_fd;
		struct stat statb;
		void * in;
		samples_odb_t dest;
		char * err_msg;
		int rc;

		assert((in_fd = open(inputs[0].c_str(), O_RDONLY)) > 0);		
		assert(fstat(in_fd, &statb)==0);
		assert((in = mmap(0, statb.st_size, PROT_READ, 
				  MAP_PRIVATE, in_fd, 0)) != (void *)-1);

		rc = odb_open(&dest, output_filename.c_str(), ODB_RDWR, 
			     sizeof(struct opd_header), &err_msg);
		if (rc != EXIT_SUCCESS) {
			cerr << "odb_open() fail:\n"
			     << err_msg << endl;
			free(err_msg);
			exit(EXIT_FAILURE);
		}

		try {
			import_from_abi(input_abi, in, statb.st_size, &dest);
		} catch (Abi_exception &e) {
			cerr << "caught abi exception: " << e.desc << endl;
		}

		odb_close(&dest);

		assert(munmap(in, statb.st_size)==0);		
	}
}
