/* Using Boost.Bloom to check occurrence of DNA sequences in a genome.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <array>
#include <boost/bloom/filter.hpp>
#include <boost/bloom/fast_multiblock32.hpp>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

/* A k-mer is a sliding segment of size k over a sequence of DNA nucleotides
 * (A, C, G, T). k_mer encodes the segment in a 64-bit word using 2 bits per
 * nucleotide.
 */

template<std::size_t K>
struct k_mer
{
  static_assert(
    K >= 0 &&
    2 * K <= sizeof(std::uint64_t) * CHAR_BIT);

  static constexpr std::size_t size()
  {
    return K;
  }

  void reset()
  {
    data = 0;
  }

  /* shift the k-mer and append a new nucleotide */

  k_mer& operator+=(char n)
  {
    static constexpr std::uint64_t mask=
      (((std::uint64_t)1) << (2 * size())) - 1;

    data <<= 2;
    data &= mask;
    data |= table[(unsigned char)n];
    return *this;
  }

  std::uint64_t data = 0;

  using table_type=std::array<unsigned char, UCHAR_MAX>;

  static constexpr table_type table = [] {
    table_type table{};
    table['A'] = table['a'] = 0;
    table['C'] = table['c'] = 1;
    table['G'] = table['g'] = 2;
    table['T'] = table['t'] = 3;
    return table;
  }();
};

template<std::size_t N>
std::size_t hash_value(const k_mer<N>& km)
{
  /* k:mer::data is 8 bytes wide. We use it directly as the associated
   * hash value in 64-bit mode, as std::size_t is the same size; in 32-bit
   * mode, we XOR the high and low portions of data to make it fit into
   * a std::size_t.
   */

  if constexpr (sizeof(std::size_t) >= sizeof(std::uint64_t)) {
    return (std::size_t)km.data;
  }
  else{ /* 32-bit mode */
    return (std::size_t)(km.data ^ (km.data >> 32));
  }
}

/* Insert all the k-mers of a given genome in a boost::bloom::filter.
 * Assumed format is FASTA with A, C, G, T.
 * https://en.wikipedia.org/wiki/FASTA_format
 */

using genome_filter = boost::bloom::filter<
  k_mer<20>, /* using k-mers of length 20 */
  1, boost::bloom::fast_multiblock32<8> >;

genome_filter make_genome_filter(const char* filename)
{
  using k_mer = genome_filter::value_type;

  std::ifstream in(filename, std::ios::ate); /* open at end to tell size */
  if(!in) throw std::runtime_error("can't open file");

  /* As a rough estimation, we assume that the number of k-mers
   * is approximately equal to the length of the genome --this is
   * overpessimistic due to the likely presence of duplicate k-mers.
   * We set FPR = 1%.
   */

  genome_filter f((std::size_t)in.tellg(), 0.01);
  in.seekg(0);

  std::string line;
  std::size_t width = 0;
  k_mer       km;
  while(std::getline(in, line)) {
    if(line.empty()) continue;
    if(line[0] == '>') { /* annotation lines in the FASTA format */
      width = 0;
      km.reset();
      continue;
    }
    std::size_t i = 0;

    /* don't insert km till it has km.size() nucleotides */

    for(; width< km.size() - 1 && i < line.size(); ++i) {
      km += line[i];
      ++width;
    }

    for(; i < line.size(); ++i) {
      km += line[i];
      f.insert(km);
    }
  }
  return f;
}

/* We estimate a DNA sequence seq to be contained in a genome if all the k-mers
 * of seq are contained. The calculation of the resulting false positive rate
 * is left as an exercise for the reader.
 */

bool may_contain(const genome_filter& f, std::string_view seq)
{
  using k_mer = genome_filter::value_type;

  assert(seq.size() >= k_mer::size());

  k_mer km;
  auto  first = seq.begin(), last = seq.end();

  /* feed first km.size() -1  nucleotides */

  for(std::size_t i = 0; i < km.size() - 1; ++i) km += *first++;

  do{
    km += *first++;
    if(!f.may_contain(km)) return false;
  }while(first != last);
  return true;
}

int main()
{
  try{
    /* Fruit fly genome (Drosophila melanogaster), available at
     * https://www.ncbi.nlm.nih.gov/datasets/genome/GCF_000001215.4/ 
     */

    auto f=make_genome_filter(
      "GCF_000001215.4_Release_6_plus_ISO1_MT_genomic.fna");

    /* Some DNA sequences */

    const char* seqs[] = {
      "ataaataagattgCGACTCAAAATTAAgcaataacac",                   /* chr.  X */
      "attatagggagaaatatgatcgcgtatgcgagagtagtgccaacatattgtgctc", /* chr. 3L */
      "agaATTTACTAAGTACTTCTATGAATGGAATTATTATTGGAAACTCTACAA",     /* chr.  4 */
      "ATTTACTAAGTACTTCTATCTGCAAATTAACAATTTATCAAACAACTG",    /* not present */
      "ataaataagattgCGACTCAAAAGTAAgcaat" /* mutation in chr. X, not present */
    };

    int i = 0;
    for(auto seq: seqs){
      std::cout << "check sequence " << i++ << ": " 
                << may_contain(f, seq) << "\n";
    }
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
