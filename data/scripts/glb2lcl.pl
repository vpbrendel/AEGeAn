#!/usr/local/env perl

# Copyright (c) 2010-2015, Daniel S. Standage and CONTRIBUTORS
#
# The AEGeAn Toolkit is distributed under the ISC License. See
# the 'LICENSE' file in the AEGeAn source code distribution or
# online at https://github.com/standage/AEGeAn/blob/master/LICENSE.
#
# glb2lcl.pl: convert LocusPocus output from sequence (chromosome or scaffold)
#             based coordinates to locus based coordinates.
#
# Usage: perl glb2lcl.pl < locus-features.gff3 > locus-features-local.gff3
use strict;

my $ilocusid;
my $ilocuspos;
my $offset;
while(<STDIN>)
{
  if(m/^#/)
  {
    print unless(m/^##sequence-region/);
    next;
  }

  my @fields = split(/\t/);
  if($fields[2] eq "locus")
  {
    ($ilocusid) = $fields[8] =~ m/ID=([^;]+)/;
    $offset = $fields[3] - 1;
    $ilocuspos = sprintf("%s_%lu-%lu", $fields[0], $fields[3], $fields[4]);
    next;
  }
  $fields[0] = $ilocusid;
  $fields[3] -= $offset;
  $fields[4] -= $offset;
  $fields[8] =~ s/Parent=$ilocusid;*//;
  $fields[8] = "iLocus_pos=$ilocuspos;". $fields[8];
  print join("\t", @fields);
}
