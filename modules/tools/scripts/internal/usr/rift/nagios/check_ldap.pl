#!/usr/bin/perl -w
####################### check_ldap.pl #######################
# Version : 1.0
# Date : 24 Jul 2007 
# Author  : De Bodt Lieven (Lieven.DeBodt at gmail.com)
# Licence : GPL - http://www.fsf.org/licenses/gpl.txt
#############################################################
#
# help : ./check_ldap.pl -h

use strict;
use Net::LDAP;
use Net::LDAPS;
use Net::LDAP::Util qw(ldap_error_text);
use Getopt::Long;

# Nagios specific

use lib "/usr/lib64/nagios/plugins/"
use utils qw(%ERRORS $TIMEOUT);
#my %ERRORS=('OK'=>0,'WARNING'=>1,'CRITICAL'=>2,'UNKNOWN'=>3,'DEPENDENT'=>4);

# Globals

my $Version='1.0';
my $Name=$0;
my $o_ldap_port = 	389; 		# ldap port
my $o_ldaps_port = 	636; 		# ldap + ssl port

my $o_host =		undef; 		# hostname 
my $o_login=		undef;		# LDAP login
my $o_help=		undef; 		# wan't some help ?
my $o_passwd=		undef;		# LDAP password
my $o_port = 		undef; 		# ldap port
my $o_version= 		undef;  	# print version
my $o_ssl=		undef; 		# use ssl
my $o_timeout=  	15;            	# Default 15s Timeout

# functions

sub show_versioninfo { print "$Name version : $Version\n"; }

sub print_usage {
    print "Usage: $Name -H <host> [-l login -x passwd] [-p <port>] [-s] [-t <timeout>] [-V]\n";
}

# Get the alarm signal (just in case ldap timout screws up)
$SIG{'ALRM'} = sub {
     print ("ERROR: Alarm signal (Nagios time-out)\n");
     exit $ERRORS{"CRITICAL"};
};

sub help {
   print "\nLDAP Monitor for Nagios version ",$Version,"\n";
   print "GPL licence, (c)2006-2007 De Bodt Lieven\n\n";
   print_usage();
   print <<EOT;
-h, --help
   print this help message
-H, --hostname=HOST
   name or IP address of host to check
-l, --login=LOGIN
   Login for ldap authentication (if not specified $Name uses anonymous)
-x, --passwd=PASSWD
   Password for ldap authentication
-p, --port=PORT
   LDAP port (Default $o_ldap_port)
-s, --ssl
   LDAPS (Default false, if true, default port is $o_ldaps_port )
-t, --timeout=INTEGER
   timeout in seconds (Default: $o_timeout)
-V, --version
   prints version number
Note :
  The script will return 
    OK if we are able to connect and bind to the LDAP server,
    WARNING if we are able to connect but not bind to the LDAP server 
    CRITICAL if we aren't able to connect to the LDAP server
EOT
}

sub check_options {
    Getopt::Long::Configure ("bundling");
    GetOptions(
        'h'     => \$o_help,    	'help'        	=> \$o_help,
        'H:s'   => \$o_host,		'hostname:s'	=> \$o_host,
        'l:s'   => \$o_login,           'login:s'       => \$o_login,
        'p:i'   => \$o_port,   		'port:i'	=> \$o_port,
        's'     => \$o_ssl,          	'ssl'      	=> \$o_ssl,
	't:i'   => \$o_timeout,       	'timeout:i'     => \$o_timeout,
	'V'     => \$o_version,         'version'       => \$o_version,
        'x:s'   => \$o_passwd,          'passwd:s'      => \$o_passwd
    );
    if (defined ($o_help)) { help(); exit $ERRORS{"UNKNOWN"}};
    if (defined($o_version)) { show_versioninfo(); exit $ERRORS{"UNKNOWN"}};
    # check ldap login information
    if ((defined($o_login) && !defined($o_passwd)) || (!defined($o_login) && defined($o_passwd)))
        { print "Put ldap login info!\n"; print_usage(); exit $ERRORS{"UNKNOWN"}}
    # Check compulsory attributes
    if ( !defined($o_host) ) { print_usage(); exit $ERRORS{"UNKNOWN"}};
}

########## MAIN #######

check_options();

my $ldap;
if (!defined($o_ssl)) {
  if (defined($o_port)) {
    $ldap = Net::LDAP->new( $o_host, port => $o_port, version => 3, timeout => $o_timeout );
  } else {
    $ldap = Net::LDAP->new( $o_host, port => $o_ldap_port, version => 3, timeout => $o_timeout );
  }
} else { 
  if (defined($o_port)) {
    $ldap = Net::LDAPS->new( $o_host, port => $o_port, version => 3, timeout => $o_timeout );
  } else {
    $ldap = Net::LDAPS->new( $o_host, port => $o_ldaps_port, version => 3, timeout => $o_timeout );
  }
}

if ($ldap) {
  my $mesg;
  if (defined($o_login) && defined($o_passwd)) {
    $mesg = $ldap->bind( $o_login, password=>$o_passwd ); # Bind to the directory server by specifying a login and password
  } else {
    $mesg = $ldap->bind();                                # Bind anonymously to the directory server
  }
  if ($mesg->code) {
    my $errstr = $mesg->code;
    $mesg = $ldap->unbind(); # Unbind from the directory server
    printf("WARNING %d: %s\n", $errstr, ldap_error_text($errstr));
    exit $ERRORS{"WARNING"};
  } else {
    $mesg= $ldap->unbind();  # Unbind from the directory server
    print "OK\n";
    exit $ERRORS{"OK"};
  }
} else {
  print "CRITICAL\n";
  exit $ERRORS{"CRITICAL"};
}
