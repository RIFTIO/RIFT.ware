#!/usr/bin/perl

#
# File: update_sep_version.pl
#
# Description: clone an existing SEP driver to new kernel version
#
# Version: 1.0
#
# Copyright (C) 2012 Intel Corporation.  All Rights Reserved.
#
#     This file is part of SEP Development Kit
#
#     SEP Development Kit is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     version 2 as published by the Free Software Foundation.
#
#     SEP Development Kit is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with SEP Development Kit; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# 
#     As a special exception, you may use this file as part of a free software
#     library without restriction.  Specifically, if other files instantiate
#     templates or use macros or inline functions from this file, or you compile
#     this file and link it with other files to produce an executable, this
#     file does not by itself cause the resulting executable to be covered by
#     the GNU General Public License.  This exception does not however
#     invalidate any other reasons why the executable file might be covered by
#     the GNU General Public License.
#

($in, $out) = @ARGV;
die "Missing input file name.\n" unless $in;
die "Missing output file name.\n" unless $out;

$in = "$ARGV[0]";
$| = 1;

#
# using conversion functions written by John Winger found at planet-source-code.com
# 
sub ascii_to_hex ($)
{
    ## Convert each ASCII character to a two-digit hex number.
    (my $str = shift) =~ s/(.|\n)/sprintf("%02lx", ord $1)/eg;
    return $str;
}

sub hex_to_ascii ($)
{
    ## Convert each two-digit hex number back to an ASCII character.
    (my $str = shift) =~ s/([a-fA-F0-9]{2})/chr(hex $1)/eg;
    return $str;
}


@vermagic_str = ("76","65","72","6d","61","67","69","63","3d");
@SMP_str = ("20","53","4d","50");

$byteCount = 0;
open(IN, "< $in");
binmode(IN);
open(OUT, "> $out");
binmode(OUT);

# get linux version on current system
$curr_linux_ascii = `uname -r`;
chomp($curr_linux_ascii);

my($hex);
my($out);
my($found) = 0;

while (read(IN,$b,1)) {
    $byteCount++;
    $hex = unpack( 'H*', $b );

    if ($found == 1) {
	$out = pack( 'H*', $hex );   
	print(OUT $out);
	next;
    }

    # parse through the 'vermagic=' string
    if ($hex eq $vermagic_str[0]) {
	$out = pack( 'H*', $hex );
	print(OUT $out);

	for ($i = 1; $i < 9; $i++) {
	    read(IN,$b,1);
	    $byteCount++;

	    # want this all transfered to the new binary
	    $hex = unpack( 'H*', $b );

	    if ($hex ne $vermagic_str[$i]) {
		last;
	    }

	    $out = pack( 'H*', $hex);
	    print(OUT $out);

	    if ($i == 8) {
		$found = 1;
	    }
	}

	# extract linux version in driver
	$drv_linux = '';
	while (($found == 1) &&
	       ($hex ne $SMP_str[0])) {
	    read(IN,$b,1);
	    $hex = unpack( 'H*', $b );
	    $byteCount++;

	    if ($hex eq $SMP_str[0]) {
		last;
	    }

	    # save the driver's version of linux for comparison
	    $drv_linux_hex = $drv_linux_hex . $hex;
	}
	$drv_linux_ascii = hex_to_ascii($drv_linux_hex);

	# compare the two and see if they're the same
	if ($drv_linux_str eq $curr_linux_ascii) {
	    print "Current linux installation matches the driver version\n";
	    
	    close(IN);
	    close(OUT);
	    
	    #delete new output file.
	    unlink($out);
	    exit;
	}

	# compatible binaries?
	# major, minor, revision numbers need to match
	if ($found == 1) {
	    # driver versioning
	    my($drv_major);
	    my($drv_minor);
	    my($drv_rev);
	    $drv_linux_ascii =~ /(\d+)\.(\d+)\.(\d+)-.*/;
	    $drv_major = $1;
	    $drv_minor = $2;
	    $drv_rev = $3;
	    
	    # linux installation versioning
	    my($curr_major);
	    my($curr_minor);
	    my($curr_rev);
	    $curr_linux_ascii =~ /(\d+)\.(\d+)\.(\d+)-.*/;
	    $curr_major = $1;
	    $curr_minor = $2;
	    $curr_rev = $3;

	    if (($drv_major ne $curr_major) ||
		($drv_minor ne $curr_minor) ||
		($drv_rev ne $curr_rev)) {
	      # we have an incompatibility
	      print "ERROR: cannot update $in to accomodate linux version $curr_linux_ascii!\n";

	      close(IN);
	      close(OUT);

	      #delete new output file
	      unlink($out);
	      exit;
	    }
	}

	# replace driver linux version with current linux installation string and continue
	if ($found == 1) {
	    $out = pack( 'H*', ascii_to_hex($curr_linux_ascii));
	    $curr_linux_hex = ascii_to_hex($curr_linux_ascii);
	    print(OUT $out);
	    # need to put space character back in
	    $out = pack ( 'H*', $hex );
	    print(OUT $out);
	}
    }

    if ($found == 0) {
	$out = pack( 'H*', $hex );   
	print(OUT $out);
    }
}
close(IN);
close(OUT);

# delete original file
unlink($in);

print "Number of bytes copied = $byteCount\n";
exit;
