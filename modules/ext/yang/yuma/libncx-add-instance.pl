#!/bin/perl

# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/06/09
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

#
# This script was run over the libncx source code to add an instance
# pointer to all the APIs.  The conversion was not completely automatic;
# numerous hand edits were required after this script ran (not the least
# because the script started one directory too shallow).  This is checked
# in for posterity, and just in case we need to run it again on a new
# version of the library.  Sorry for all the ugly hacks - Tom.
#

use strict vars;
use strict subs;
our $x=0;

#our @src = (
#  "x f( f y ) { junkl }",
#  "x f() { junkl }",
#  "x f( f y ) {}",
#  "x f( f y ) { }",
#  "x f( f y ) {{}}",
#  "x f( f y ) {{} }",
#  "x f( (f) y ) { junkl }",
#  "x f( f y ) { if (x) { junk } }",
#  "x f( v(*)(dd d), y ) { if (x) { junk } }",
#  "x f( v(*)(dd d), y ) { junk }",
#  "x f( f y ) { if (x) { junk } else { c } }",
#);
#for my $src ( @src )
#{
#   my $fn = 'f';
#   #f( $src =~ m/\b$fn\b\s*(\((?>[^()]+|(?-1))\))\s*(\{(?>[^{}]+|(?-1))\})/s )
#   #f( $src =~ m/\b$fn\b\s*(\((?:[^()]+|(?-1))\))\s*(\{(?>[^{}]+|(?-1))\})/s )
#   #f( $src =~ m/\b$fn\b\s*(\((?:[^()]+(?-1)?[^()]*)\))\s*(\{(?>[^{}]+|(?-1))\})/s )
#   #f( $src =~ m/\b$fn\b\s*(\((?>[^()]+(?-1)?[^()]*)\))\s*(\{(?>[^{}]+|(?-1))\})/s )
#   #f( $src =~ m/\b$fn\b\s*(\((?>[^()]+(?-1)*[^()]*)\))\s*(\{(?>[^{}]+(?-1)?[^{}]*)\})/s )
#   #f( $src =~ m/\b$fn\b\s*(\((?>[^()]+(?-1)*[^()]*)\))\s*(\{(?>[^{}]*(?-1)*)+\})/s )
#   # (\{(?>[^{}]*(?-1)*)+\})
#   # (\((?>[^()]*(?-1)*)+\))
#   if( $src =~ m/\b$fn\b\s*(\((?>[^()]*(?-1)*)+\))\s*(\{(?>[^{}]*(?-1)*)+\})/s )
#   {
#     print "'$src': '$1' '$2'\n";
#   }
#   else
#   {
#     print "'$src': nope\n"
#   }
#}
#exit 1;

#our @src = (
#  " f((const xmlChar *)user); ",
#  " f(target); ",
#);
#for my $src ( @src )
#{
#   my $fn = 'f';
#   if( $src =~ m/\b$fn\s*(\((?>[^()]*(?-1)*)+\))/s )
#   {
#     print "'$src': '$1' '$2'\n";
#   }
#   else
#   {
#     print "'$src': nope\n"
#   }
#}
#exit 1;


our @statics = qw(
  allow_top_mandatory altlogfile auditlogfile basetypes cfg_arr cfg_init_done
  cwd_subdirs deadmodQ debug_level def_reg_init_done defcxt display_mode
  error_count error_level error_stack feature_code_default feature_enable_default
  feature_entryQ feature_init_done freeQ free_cnt freecnt gen_anyxml gen_binary
  gen_container gen_empty gen_root gen_string inreadyQ last_id logfile malloc_cnt
  mod_load_callback ncx_curQ ncx_cur_filptrs ncx_filptrQ ncx_init_done
  ncx_max_filptrs ncx_modQ ncx_sesmodQ ncxmod_alt_path ncxmod_data_path
  ncxmod_data_path_cli ncxmod_env_install ncxmod_home ncxmod_home_cli
  ncxmod_init_done ncxmod_mod_path ncxmod_mod_path_cli ncxmod_run_path
  ncxmod_run_path_cli ncxmod_subdirs ncxmod_yuma_home ncxmod_yuma_home_cli
  ncxmod_yumadir_path outreadyQ protocols_enabled runstack_init_done save_descr
  ses_msg_init_done staterr staterr_inuse system_sorted temp_modQ topQ
  top_init_done topht totals tracefile typ_init_done use_audit_tstamps
  use_prefix use_tstamps usedeadmodQ warn_idlen warn_linelen warnoffQ xmlns
  xmlns_init_done xmlns_invid xmlns_ncid xmlns_ncnid xmlns_ncxid xmlns_next_id
  xmlns_nsid xmlns_wdaid xmlns_wildcardid xmlns_xmlid xmlns_xsid xmlns_xsiid
  xmlns_yangid xmlns_yinid 
);


our @globals = qw(
  calloc free malloc realloc open close printf syslog vprintf fflush
  LOGERROR LOGWARN LOGINFO LOGDEBUG LOGDEBUG2 LOGDEBUG3 LOGDEBUG4
);

chdir "$ENV{RIFT_ROOT}/modules/ext/yang/yuma/yuma123-2.2.5/netconf/src";


# Slurp the source files into memory.
print STDERR "Slurp source...";
our @findargs = qw(-type f -name *.[ch] -print);
open my $srcs_fh, "-|", 'find', '.',  @findargs;
our %files;
our %orig;
while( my $sf = <$srcs_fh> )
{
  print STDERR "." if 0 == (($x++) % 5);
  chomp $sf;
  system "git checkout $sf" and die "unable to checkout $sf";
  open my $in_fh, "<$sf" or die "unable to open $sf";
  $files{$sf} = join "", <$in_fh>;
  $orig{$sf} = $files{$sf};
}
close $srcs_fh;
print STDERR "\n";


print STDERR "Preprocessor suppress...";
for my $sf ( sort keys %files )
{
  my $l = length($files{$sf});
  print STDERR "." if 0 == (($x++) % 5);

  # Join escaped newlines
  $files{$sf} =~ s/\\\n/  /sg;

  # Remove strings
  $files{$sf} =~ s[("(?:[^\\""\n]|\\.)*?")]
                  ['""' . (' ' x (length($1)-2))]sge;

  # Remove character constants
  $files{$sf} =~ s[('(?:[^\\]|\\.|\\[a-zA-Z]|\\[0-9]{1,3}|\\[Xx][a-fA-F0-9]{1,2})')]
                  ["'x'" . (' ' x (length($1)-3))]sge;

  # Remove C comments
  $files{$sf} =~ s/(\/\*.*?\*\/)/' ' x length($1)/sge;

  # Remove C++ comments
  $files{$sf} =~ s/([ \ci]*\/\/[^\n]*?)\n/' ' x length($1) . "\n"/sge;

  ## Change NUL, tab, CR, VT, and FF to space; it simplifies processing
  #$files{$sf} =~ s/[\cl\ci\ck\cm\x00]/ /gs;

  die "Preprocessor fix broken '$sf'" unless $l == length($files{$sf});
}
print STDERR "\n";


print STDERR "Find function declarations...";
our @fdecls;
our %fdecls;
for my $sf ( sort keys %files )
{
  while ($files{$sf} =~ m/\n\s*\b(\bstatic\b|\bextern\b)\s*([^;{}]+)\s*(\b\w+\b)\s*(\((?>[^()]*(?-1)*)+\))\s*\;/gs )
  {
    print STDERR "." if 0 == (($x++) % 100);

    my ($storcl, $rettyp, $fn, $args) = ($1, $2, $3, $4);
    my $argsl = [$-[4], $+[4]];
    #print "$sf: storcl=$storcl rettyp=$rettyp fn=$fn args=$args @{$argsl}\n";

    my $r =
    {
      t => '3.fdecl',
      sf => $sf,
      storcl => $storcl,
      rettyp => $rettyp,
      fn => $fn,
      args => $args,
      argsl => $argsl,
    };
    $fdecls{$fn} = 1;
    push @fdecls, $r;
  }
}
print STDERR "\n";


print STDERR "Find undeclared static functions...";
for my $sf ( sort keys %files )
{
  while ($files{$sf} =~ m/\n\s*\b(\bstatic\b)\s*([^;{}]+)\s*(\b\w+\b)\s*(\((?>[^()]*(?-1)*)+\))\s*\{/gs )
  {
    print STDERR "." if 0 == (($x++) % 100);

    my ($storcl, $rettyp, $fn, $args) = ($1, $2, $3, $4);
    my $argsl = [$-[4], $+[4]];
    #print "$sf: storcl=$storcl rettyp=$rettyp fn=$fn args=$args @{$argsl}\n";

    $fdecls{$fn} = 1;
  }
}
print STDERR "\n";

print STDERR "Find function-like macros...";
our %mdefns;
our @mdefns;
for my $sf ( keys %files )
{
  while ($files{$sf} =~ m/\n\s*\#define\s+(\w+)(\((?>[^()]*(?-1)*)+\))\s+([^\n]*)\n/gs )
  {
    print STDERR "." if 0 == (($x++) % 10);
    my ($fn, $args, $code) = ($1, $2, $3);
    my $argsl = [$-[2], $+[2]];
    my $codel = [$-[3], $+[3]];
    #print "$sf: fn=$fn, @$argsl args=$args, code=$code\n";

    my $r =
    {
      t => '4.mdefn',
      sf => $sf,
      fn => $fn,
      args => $args,
      argsl => $argsl,
      code => $code,
      codel => $codel,
      callto => [],
      globals => [],
      datause => [],
    };
    $mdefns{$fn} = 1;
    push @mdefns, $r;
  }
}
print STDERR "\n";


print STDERR "Build function/macro name list...";
our %funcs;
foreach my $fn ( sort keys(%fdecls), keys(%mdefns) )
{
  print STDERR "." if 0 == (($x++) % 100);
  if (exists $funcs{$fn} )
  {
    #print STDERR "Duplicate macro/function: $fn()\n";
  }
  else
  {
    $funcs{$fn} =
    {
      callfrom => {},
      needs_instance => 0,
    };
  }
}
print STDERR "\n";


print STDERR "Find function definitions...";
our @fdefns;
for my $sf ( sort keys %files )
{
  my $src = $files{$sf};

  # Search for each function definition
  for my $fn ( sort keys %funcs )
  {
    # Found a function def?
    while( $src =~ m/\b$fn\b\s*(\((?>[^()]*(?-1)*)+\))\s*(\{(?>[^{}]*(?-1)*)+\})/gs )
    {
      print STDERR "." if 0 == (($x++) % 100);

      my( $args, $code ) = ( $1, $2 );
      my $argsl = [$-[1], $+[1]];
      my $codel = [$-[2], $+[2]];
      #print "$sf: $fn: argsl=@$codel, codel=@$codel, args=$args, code=$code\n";

      my $r =
      {
        t => '5.fdefn',
        sf => $sf,
        fn => $fn,
        args => $args,
        argsl => $argsl,
        code => $code,
        codel => $codel,
        callto => [],
        globals => [],
        datause => [],
      };

      push @fdefns, $r;
    }
  }
}
print STDERR "\n";


print STDERR "Find uses...";
our @callto;
our @datause;
sub find_uses
{
  my $r = $_[0];
  my $fn = $r->{fn};
  my $sf = $r->{sf};
  my $loc = $r->{codel}[0];
  #print "$fn: find_uses\n";
  print STDERR "." if 0 == (($x++) % 100);

  # Has local statics?
  if( $r->{code} =~ /\bstatic\b/ )
  {
    $r->{local_statics} = 1;
  }

  # Search for each known static variable within the function body
  for my $var ( @statics )
  {
    while( $r->{code} =~ /(?<!\.)(?<!\-\>)\s*\b($var)\b/gs )
    {
      #print "$fn: static: $1\n";
      my $c =
      {
        t => "2.datause $var",
        sf => $sf,
        fn => $fn,
        fn_r => $r,
        var => $var,
        varl => [$loc+$-[1], $loc+$+[1]],
      };

      push @{$r->{datause}}, $c;
      push @datause, $c;
      $funcs{$fn}{needs_instance}++;
    }
  }

  # Search for each global function within the function body
  for my $global ( @globals )
  {
    if( $r->{code} =~ /(?<!\.)(?<!\-\>)\s*\b($global)\b/ )
    {
      #print "$fn: global: $1\n";
      $r->{globals} = 1;
      $funcs{$fn}{needs_instance}++;
    }
  }

  # Search for the other functions names within the function body
  for my $ofn ( sort keys %funcs )
  {
    while( $r->{code} =~ /\b$ofn\s*(\((?>[^()]*(?-1)*)+\))/sg )
    {
      #print "$fn: calls: $ofn $1\n";
      my $c =
      {
        t => "1.$fn calls $ofn",
        sf => $sf,
        fn => $fn,
        fn_r => $r,
        args => $1,
        argsl => [$loc+$-[1], $loc+$+[1]],
        callto_fn => $ofn,
      };

      push @{$r->{callto}}, $c;
      $funcs{$ofn}{callfrom}{$fn} = 1;
      push @callto, $c;
    }
  }
}
for my $r ( @mdefns ) { find_uses($r) }
for my $r ( @fdefns ) { find_uses($r) }
print STDERR "\n";


# Build the recursive closure of all callers:
# f() calls g() calls h() => f() calls g()
print STDERR "Calculate call closure...";
for my $fn ( sort keys %funcs )
{
  print STDERR "." if 0 == (($x++) % 100);
  my %callfrom = %{$funcs{$fn}{callfrom}};
  delete $callfrom{$fn};
  my @callfrom = sort keys %callfrom;
  $callfrom{$fn} = 1;

  while( my $ofn = pop @callfrom )
  {
    $funcs{$ofn}{needs_instance} |= $funcs{$fn}{needs_instance};

    # Put my caller's callers... on the callers list, if not already.
    # Ultimately, this will build the entire caller callfrom, back to the root
    foreach my $fn3 ( sort keys %{$funcs{$ofn}{callfrom}} )
    {
      #print "$fn: callfrom: $ofn: $fn3\n";
      $funcs{$fn}{callfrom}{$fn3} = 1;
      push @callfrom, $fn3 unless exists $callfrom{$fn3};
      $callfrom{$fn3} = 1;
      $funcs{$fn3}{needs_instance} |= $funcs{$fn}{needs_instance};
    }
  }
}
print STDERR "\n";


# Find missing defs
for my $fn ( sort keys %funcs )
{
  #print "need instance: $fn()\n" if $funcs{$fn}{needs_instance};
}


sub insert_prototype_arg
{
  my( $sf, $t, $loc, $args, $def ) = @_;
  my $remove = 0;
  my $insert = "$def *instance";

  if ( $args =~ /\A(\(\s*)void\s*\)\z/s )
  {
    $remove = 4;
    $loc += length($1);
  }
  elsif ( $args =~ /\A(\()\s*\)\z/s )
  {
    $loc += length($1);
  }
  elsif ( $args =~ /\A(\()\s*+[^,]*\)\z/s )
  {
    $insert .= ", ";
    $loc += length($1);
  }
  else
  {
    $args =~ /\A(\()\s*.*?\,(\s*).*\)\z/s or die "Could not parse argument list: $sf: $loc: '$args'";
    $insert .= "," . $2;
    $loc += length($1);
  }

  my $e =
  {
    sf => $sf,
    t => $t,
    remove => $remove,
    loc => $loc,
    insert => $insert,
  };
  return $e;
}

sub insert_instance_arg
{
  my( $sf, $t, $loc, $args ) = @_;
  my $insert = 'instance';

  if ( $args =~ /\A(\()\s*\)\z/s )
  {
    $loc += length($1);
  }
  elsif ( $args =~ /\A(\()\s*+[^,]*\)\z/s )
  {
    $insert .= ", ";
    $loc += length($1);
  }
  else
  {
    $args =~ /\A(\()\s*.*?\,(\s*).*\)\z/s or die "Could not parse argument list: $sf: $loc: '$args'";
    $insert .= "," . $2;
    $loc += length($1);
  }

  my $e =
  {
    sf => $sf,
    t => $t,
    remove => 0,
    loc => $loc,
    insert => $insert,
  };
  return $e;
}


# To add args to:
#   C-decl: add instance pointer arg: struct ncx_instance_t_ *instance[,][\s] {-void}
#   C-defn: add instance pointer arg: ncx_instance_t *instance[,][\s] {-void}
#   M-decl: add instance arg, bare word: instance[,][\s]
#   invoke: add instance arg, bare word: instance[,][\s]
#   use: add instance dereference: instance->
#
# ncx_instance_t *ncxinst
our @edits;
print STDERR "Find function arg declaration edits...";
for my $r ( @fdecls )
{
  my $fn = $r->{fn};
  next unless $funcs{$fn}{needs_instance};
  print STDERR "." if 0 == (($x++) % 100);

  my $e = insert_prototype_arg( $r->{sf}, $r->{t}, $r->{argsl}[0], $r->{args}, 'struct ncx_instance_t_' );

  #print "$r->{sf}: $fn: edit: $r->{args}: $e->{loc} $e->{remove} '$e->{insert}'\n";
  push @edits, $e;
}
print STDERR "\n";


print STDERR "Find function arg definition edits...";
for my $r ( @fdefns )
{
  my $fn = $r->{fn};
  next unless $funcs{$fn}{needs_instance};
  print STDERR "." if 0 == (($x++) % 100);

  my $e = insert_prototype_arg( $r->{sf}, $r->{t}, $r->{argsl}[0], $r->{args}, 'ncx_instance_t' );

  #print "$r->{sf}: $fn: edit: $r->{args}: $e->{loc} $e->{remove} '$e->{insert}'\n";
  push @edits, $e;
}
print STDERR "\n";


print STDERR "Find macro declaration edits...";
for my $r ( @mdefns )
{
  my $fn = $r->{fn};
  next unless $funcs{$fn}{needs_instance};
  print STDERR "." if 0 == (($x++) % 100);

  my $e = insert_instance_arg( $r->{sf}, $r->{t}, $r->{argsl}[0], $r->{args} );

  #print "$r->{sf}: $fn: edit: $r->{args}: $e->{loc} $e->{remove} '$e->{insert}'\n";
  push @edits, $e;
}
print STDERR "\n";


print STDERR "Find invocation edits...";
for my $r ( @callto )
{
  my $fn = $r->{callto_fn};
  next unless $funcs{$fn}{needs_instance};
  print STDERR "." if 0 == (($x++) % 400);

  my $e = insert_instance_arg( $r->{sf}, $r->{t}, $r->{argsl}[0], $r->{args} );

  #print "$r->{sf}: $fn: edit: $r->{args}: $e->{loc} $e->{remove} '$e->{insert}'\n";
  push @edits, $e;
}
print STDERR "\n";


print STDERR "Find static reference edits...";
for my $r ( @datause )
{
  my $fn = $r->{fn};
  print STDERR "." if 0 == (($x++) % 100);

  my $e =
  {
    t => $r->{t},
    sf => $r->{sf},
    remove => 0,
    loc => $r->{varl}[0],
    insert => 'instance->',
  };

  #print "$r->{sf}: $fn: edit: $r->{var}: $e->{loc} '$e->{insert}'\n";
  push @edits, $e;
}
print STDERR "\n";


# ATTN: Sort edit list by file and by descending string index
print STDERR "Make edits...";
our %edits;
our %edited;
for my $e ( @edits )
{
  my $sf = $e->{sf};
  my $k = sprintf "%06u.%s", $e->{loc}, $e->{t};
  if( exists $edits{$sf}{$k} )
  {
    die "dup edit: $sf $k";
  }
  $edits{$sf}{$k} = $e;
}
for my $sf ( sort keys %edits )
{
  my $src = $orig{$sf};
  for my $k ( reverse sort keys %{$edits{$sf}} )
  {
    print STDERR "." if 0 == (($x++) % 500);

    my $e = $edits{$sf}{$k};
    my $loc = $e->{loc};
    my $len = length($e->{insert});

    substr $src, $loc, $e->{remove}, $e->{insert};
    #print "$sf: $loc $k: '", substr($orig{$sf},$loc-24,$len+48), "', '", substr($src,$loc-24,$len+48), "'\n";
  }
  $edited{$sf} = $src;
}
print STDERR "\n";


# Save intermediate files.
print STDERR "Save changed files...";
for my $sf ( sort keys %edited )
{
  print STDERR "." if 0 == (($x++) % 5);
  open my $out_fh, ">$sf";
  print $out_fh $edited{$sf};
}
print STDERR "\n";


