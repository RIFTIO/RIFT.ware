#!/bin/perl
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/02/13
# 
# This file generates RW.CmdArgs glue source files.

use strict;
our $p = $0;
$p =~ s/.*+\///;

sub ONE_ARG { 1 }
sub MULTI_ARG { 2 }
sub help;
our %options =
(
    '--help'       => 'help',
    '-help'        => 'help',
    '-h'           => 'help',
    help           => [ \&help, "Display help" ],
    BASE           => [ ONE_ARG, "Output source files base name" ],
    CMDARGS_F_NAME => [ ONE_ARG, "Function name to define that does all the work" ],
    INSTALL_COMP   => [ ONE_ARG, "Install component (for documentation)" ],
    PCC_TYPE       => [ ONE_ARG, "Protobuf-c Message type name for configuration data structure" ],
    PCC_H_FILES    => [ MULTI_ARG, "Protobuf-c include files - must exist, will be read" ],
    XSD_FILES      => [ MULTI_ARG, "XSD schema files" ],
    YANG_FILES     => [ MULTI_ARG, "Yang files" ],
);

sub help
{
    print "$p: Generate RW.CmdArgs source files.\n";
    print "\n";
    print "USAGE: $p OPTIONS...\n";
    print "\n";
    print "OPTIONS:\n";
    foreach my $opt ( sort keys %options )
    {
        my $type, my $help;
        if( ! ref $options{$opt} )
        {
            $type = 0;
            $help = "Same as '$options{$opt}'";
        }
        else
        {
            ( $type, $help ) = @{$options{$opt}};
        }

        if( ref($type) or $type == 0 )
        {
            printf "    %-20.20s %s\n", $opt, $help;
        }
        elsif( $type == 1 )
        {
            printf "    %-20.20s %s\n", "$opt arg", $help;
        }
        else
        {
            printf "    %-20.20s %s\n", "$opt args...", $help;
        }
    }
    exit 0;
}

#############################################################################
our $hfile = <<'EOHFILE';
/**
 * This is an auto-generated file - DO NOT EDIT
 * RW.CmdArgs glue function header for @INSTALL_COMP@
 */

#ifndef @INCGUARD@
#define @INCGUARD@

#include <sys/cdefs.h>
#include "protobuf-c/rift-protobuf-c.h"
@INCLUDES@
#include "rwlib.h"
#include "rwcmdargs.h"

__BEGIN_DECLS
rw_status_t @CMDARGS_F_NAME@_init(rwcmdargs_t** rwca, bool_t want_debug);
rw_status_t @CMDARGS_F_NAME@(int* argc, char** argv, RWPB_T_MSG(@PCC_TYPE@)* pbm, bool_t want_debug);
__END_DECLS

#endif /* ifndef @INCGUARD@ */
EOHFILE
#############################################################################

#############################################################################
our $cfile = <<'EOCFILE';
/**
 * This is an auto-generated file - DO NOT EDIT
 * RW.CmdArgs glue function for @INSTALL_COMP@
 */

#include "@BASE@.h"

/**
 * RW.CmdArgs initialization for @INSTALL_COMP@
 */
rw_status_t @CMDARGS_F_NAME@_init(rwcmdargs_t** rwca, bool_t want_debug)
{
  *rwca = rwcmdargs_alloc(want_debug);
  if (!*rwca) {
    return RW_STATUS_FAILURE;
  }

  static const rwcmdargs_yang_entry_t yang_list[] =
  {
@YANG_LIST@
  };
  rw_status_t rs = rwcmdargs_load_yang_list(*rwca, yang_list, sizeof(yang_list)/sizeof(yang_list[0]));
  if (RW_STATUS_SUCCESS != rs) {
    goto fail;
  }
  rs = RW_STATUS_SUCCESS;

fail:
  if (rs != RW_STATUS_SUCCESS && *rwca) {
    rwcmdargs_free(*rwca);
    *rwca = NULL;
  }
  return rs;
}

/**
 * RW.CmdArgs one-shot invocation function for @INSTALL_COMP@
 */
rw_status_t @CMDARGS_F_NAME@(int* argcp, char** argv, RWPB_T_MSG(@PCC_TYPE@)* pbcm, bool_t want_debug)
{
  rwcmdargs_t* rwca =NULL;
  rw_status_t rs = @CMDARGS_F_NAME@_init(&rwca,want_debug);
  if (RW_STATUS_SUCCESS != rs) {
    return rs;
  }

  rs = rwcmdargs_parse_env(rwca);
  if (RW_STATUS_SUCCESS != rs) {
    goto fail;
  }

  rs = rwcmdargs_parse_args(rwca, argcp, argv);
  if (RW_STATUS_SUCCESS != rs) {
    goto fail;
  }

  rs = rwcmdargs_save_to_pb(rwca, &pbcm->base);
  if (RW_STATUS_SUCCESS != rs) {
    goto fail;
  }
  rs = RW_STATUS_SUCCESS;

fail:
  if (rwca) {
    rwcmdargs_free(rwca);
  }
  return rs;
}
EOCFILE
#############################################################################

# Parse the arguments.
our %subs = ();
argv: while( scalar(@ARGV) )
{
    my $arg = shift @ARGV;
    exists($options{$arg}) or die "$p: Error: Unrecognized option '$arg'.\n";

    my $type = $options{$arg};
    while( ! ref $type )
    {
        exists($options{$type}) or die "$p: BUG: no redirection to '$type'.\n";
        $type = $options{$type};
    }

    ( $type, my $help ) = @$type;
    if( $type == 1 )
    {
        scalar(@ARGV) or die "$p: Error: Value required for $arg: $help.\n";
        $subs{$arg} = shift @ARGV;
        next argv;
    }

    if( $type == 2 )
    {
        scalar(@ARGV) or die "$p: Error: Value(s) required for $arg: $help.\n";
        $subs{$arg} = [] unless ref $subs{$arg};
        while( scalar(@ARGV) and ! exists $options{$ARGV[0]} )
        {
            push @{$subs{$arg}}, shift @ARGV;
        }
        next argv;
    }

    ref $type or die "$p: BUG: not ref '$type'.\n";
    $type->( $arg );
}

our $basename = $subs{'BASE'};
($basename ne '') or die "$p: Error: No base filename provided!\n";

# Build the list of structures to register.
foreach my $inc ( @{$subs{'PCC_H_FILES'}} )
{
    my $nopath_inc = $inc;
    $nopath_inc =~ s/\A(?:.*?\/)?([^\/]+)\z/$1/ or die "$p: Error: Cannot string path from '$inc'\n";
    $subs{'INCLUDES'} .= "#include \"$nopath_inc\"\n";
}
delete $subs{'PCC_H_FILES'};

# Build the list of structures to register.
scalar(@{$subs{'XSD_FILES'}}) == scalar(@{$subs{'YANG_FILES'}})
  or die "$p: YANG_FILES and XSD_FILES must list the same modules\n";

# ATTN: XSDs are historical artifacts, but we are not quite ready to delete them from the source yet...
foreach my $xsd ( @{$subs{'XSD_FILES'}} )
{
    my $fh;
    my $yang = shift @{$subs{'YANG_FILES'}};
    -f "$xsd" or die "$p: Error: Unable to find '$xsd'\n";
    -f "$yang" or die "$p: Error: Unable to find '$yang'\n";
    my $module = $yang;
    $module =~ s/\A(?:.*?\/)?([^\/]+)\.yang\z/$1/ or die "$p: Error: Cannot get module from '$yang'\n";
    my $xsd_mod = $xsd;
    $xsd_mod =~ s/\A(?:.*?\/)?([^\/]+)\.xsd\z/$1/ or die "$p: Error: Cannot get module from '$xsd'\n";
    $xsd_mod eq $module or die "$p: YANG_FILES and XSD_FILES must be in the same order\n";
    $subs{'YANG_LIST'} .= "    {\n      .module = \"$module\",\n    },\n";
}
delete $subs{'XSD_FILES'};
delete $subs{'YANG_FILES'};

my $incguard = "RWCMDARGS_$subs{'INSTALL_COMP'}_$subs{'CMDARGS_F_NAME'}_H_";
$incguard =~ s/[-_.=]+/_/g;
$incguard =~ s/\W//g;
$incguard =~ s/^_*+(.*?)_*+$/${1}_/;
$subs{'INCGUARD'} = uc $incguard;

foreach my $var ( keys %subs )
{
    my $nometa = quotemeta($var);
    my $s = 0;
    $cfile =~ s/\@$nometa\@/$subs{$var}/gm and ++$s;
    $hfile =~ s/\@$nometa\@/$subs{$var}/gm and ++$s;
    $s or die "$p: Error: Extra pattern '$var'.\n";
}

$cfile =~ /\@(.*?)\@/ and die "$p: Error: Missing pattern '$1'.\n";
$hfile =~ /\@(.*?)\@/ and die "$p: Error: Missing pattern '$1'.\n";

my $fh;
open($fh, ">$basename.h") or die "$p: Error: Unable to create $basename.h\n";
print($fh $hfile) or die "$p: Error: Unable to write to $basename.h\n";
close $fh;

open($fh, ">$basename.c") or die "$p: Error: Unable to create $basename.c\n";
print($fh $cfile) or die "$p: Error: Unable to write to $basename.c\n";
close $fh;

