#!/usr/bin/perl
# nagios: +epn
#
# check_ups - nagios plugin 
#
# Copyright (C) 2006 Larry Low
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# Returns CRITICAL on most conditions.
# Returns OK on normal output and batter status.
# Returns WARNING on booster and reducer output conditions.
# Returns WARNING on low or depleted battery condition.
#
# Report bugs to:  llow0@yahoo.com
#
# Primary MIB reference - UPS-MIB
#
#
use strict;
use warnings;
use lib "/usr/lib/nagios/plugins"  ;
use utils qw($TIMEOUT %ERRORS &print_revision &support);
use vars qw($PROGNAME);

# Just in case of problems, let's not hang Nagios
$SIG{'ALRM'} = sub {
	print ("ERROR: Plugin took too long to complete (alarm)\n");
	exit $ERRORS{"UNKNOWN"};
};
alarm($TIMEOUT);

$PROGNAME = "check_ups.pl";
sub print_help ();
sub print_usage ();
use POSIX qw(floor);

my ($opt_h,$opt_V);
my $community = "public";
my $snmp_version = 2;
my ($hostname);
my ($warning) = 50;
my ($critical) = 60;

use Getopt::Long;
&Getopt::Long::config('bundling');
GetOptions(
	"V"   => \$opt_V,	"version"    => \$opt_V,
	"h"   => \$opt_h,	"help"       => \$opt_h,
	"C=s" => \$community,	"community=s" => \$community,
	"H=s" => \$hostname,	"hostname=s" => \$hostname,
	"v=i" => \$snmp_version,"snmp_version=i" => \$snmp_version,
	"w=i" => \$warning, "warning_level=i", => \$warning,
	"c=i" => \$critical, "critical_level=i", => \$critical
);
# -h & --help print help
if ($opt_h) { print_help(); exit $ERRORS{'OK'}; }
# -V & --version print version
if ($opt_V) { print_revision($PROGNAME,'$Revision: 0.1 $ '); exit $ERRORS{'OK'}; }
# Invalid hostname print usage
if (!utils::is_hostname($hostname)) { print_usage(); exit $ERRORS{'UNKNOWN'}; }

# Setup SNMP object
use Net::SNMP qw(INTEGER OCTET_STRING IPADDRESS OBJECT_IDENTIFIER NULL);
my ($snmp, $snmperror);
if ($snmp_version == 2) {
	($snmp, $snmperror) = Net::SNMP->session(
		-hostname => $hostname,
		-version => 'snmpv2c',
		-community => $community
	);
} elsif ($snmp_version == 3) {
	my ($v3_username,$v3_password,$v3_protocol,$v3_priv_passphrase,$v3_priv_protocol) = split(":",$community);
	my @auth = ();
	if (defined($v3_password)) { push(@auth,($v3_password =~ /^0x/) ? 'authkey' : 'authpassword',$v3_password); }
	if (defined($v3_protocol)) { push(@auth,'authprotocol',$v3_protocol); }
	if (defined($v3_priv_passphrase)) { push(@auth,($v3_priv_passphrase =~ /^0x/) ? 'privkey' : 'privpassword',$v3_priv_passphrase); }
	if (defined($v3_priv_protocol)) { push(@auth,'privprotocol',$v3_priv_protocol); }

	($snmp, $snmperror) = Net::SNMP->session(
		-hostname => $hostname,
		-version => 'snmpv3',
		-username => $v3_username,
		@auth
	);
} else {
	($snmp, $snmperror) = Net::SNMP->session(
		-hostname => $hostname,
		-version => 'snmpv1',
		-community => $community
	);
}

if (!defined($snmp)) {
	print ("UNKNOWN - SNMP error: $snmperror\n");
	exit $ERRORS{'UNKNOWN'};
}

my $state = 'UNKNOWN';
my $output = "UPS status retrieval failed.";
# Begin plugin check code
{
	my $upsBatteryStatus = "1.3.6.1.2.1.33.1.2.1.0";
	my $upsOutputSource = "1.3.6.1.2.1.33.1.4.1.0";
	my $upsOutputPercentLoad =  "1.3.6.1.2.1.33.1.4.4.1.5.1";



	my %upsBatteryStatuses = (
		1 => "unknown(1)",
		2 => "batteryNormal(2)",
		3 => "batteryLow(3)",
		4 => "batteryDepleted(4)"
	);

	my %upsOutputSources = (
		1 => "other(1)",
		2 => "none(2)",
		3 => "normal(3)",
		4 => "bypass(4)",
		5 => "battery(5)",
		6 => "booster(6)",
		7 => "reducer(7)"
	);

	my @snmpoids;
	push (@snmpoids,$upsBatteryStatus);
	push (@snmpoids,$upsOutputSource);
	push (@snmpoids,$upsOutputPercentLoad);

	my $result = $snmp->get_request(
		-varbindlist => \@snmpoids
	);

	if (!defined($result)) {
		my $answer = $snmp->error;
		$snmp->close;
		print ("UNKNOWN - SNMP error: $answer\n");
		exit $ERRORS{'UNKNOWN'};
	}

	$upsBatteryStatus = $result->{$upsBatteryStatus};
	$upsOutputSource = $result->{$upsOutputSource};
	$upsOutputPercentLoad = $result->{$upsOutputPercentLoad};
	if (($upsOutputSource !~ /^noSuch/) &&
		($upsBatteryStatus !~ /^noSuch/)) {
		$output = "UPS State is ".$upsOutputSources{$upsOutputSource}.". Battery State is ".$upsBatteryStatuses{$upsBatteryStatus}.".";

		if ($upsOutputSource == 3) { # normal
			$state = 'OK';
		} elsif (($upsOutputSource == 6) || # booster
			($upsOutputSource == 7)) { # reducer
			$state = 'WARNING';
		} else {
			$state = 'CRITICAL';
		}

		if (($state eq 'OK') && ($upsBatteryStatus != 2)) { # battery low or depleted
			$state = 'WARNING';
		}
	}
	if ($upsOutputPercentLoad !~ /noSuch/ ) { 
		$upsOutputPercentLoad = $upsOutputPercentLoad / 10;
		$output .= " load is $upsOutputPercentLoad%.";
		if ( $upsOutputPercentLoad > $warning ) { 
			$state = 'WARNING' if $state eq 'OK';
		}
		if ( $upsOutputPercentLoad > $critical ) { 
			$state = 'CRITICAL';
		}
	}
}
print "$state - $output\n";
exit $ERRORS{$state};

sub print_help() {
	print_revision($PROGNAME,'$Revision: 0.1 $ ');
	print "Copyright (c) 2006 Larry Low\n";
	print "This program is licensed under the terms of the\n";
	print "GNU General Public License\n(check source code for details)\n";
	print "\n";
	printf "Check UPS status via SNMP.\n";
	print "\n";
	print_usage();
	print "\n";
	print " -H (--hostname)     Hostname to query - (required)\n";
	print " -C (--community)    SNMP read community or v3 auth (defaults to public)\n";
	print "                     (v3 specified as username:authpassword:... )\n";
	print "                       username = SNMPv3 security name\n";
	print "                       authpassword = SNMPv3 authentication pass phrase (or hexidecimal key)\n";
	print "                       authprotocol = SNMPv3 authentication protocol (md5 (default) or sha)\n";
	print "                       privpassword = SNMPv3 privacy pass phrase (or hexidecmal key)\n";
	print "                       privprotocol = SNMPv3 privacy protocol (des (default) or aes)\n";
	print " -v (--snmp_version) 1 for SNMP v1\n";
	print "                     2 for SNMP v2c (default)\n";
	print "                     3 for SNMP v3\n";
	print " -V (--version)      Plugin version\n";
	print " -h (--help)         usage help\n";
	print "\n";
	support();
}

sub print_usage() {
	print "Usage: \n";
	print "  $PROGNAME -H <HOSTNAME> [-C <community>]\n";
	print "  $PROGNAME [-h | --help]\n";
	print "  $PROGNAME [-V | --version]\n";
}

