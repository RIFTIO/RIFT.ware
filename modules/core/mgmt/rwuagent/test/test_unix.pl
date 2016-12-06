#/bin/perl
use IO::Socket;
use IO::Socket::UNIX;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);

$s = new IO::Socket::UNIX(Type=>SOCK_STREAM,Peer=>"$ENV{RIFT_ROOT}/.install/rw-uAgent.sock");
die "Could not connect" unless $s;
$flags = fcntl($s, F_GETFL, 0) or die "F_GETFL";
$flags = fcntl($s, F_SETFL, $flags | O_NONBLOCK) or die "F_SETFL";

$flags = fcntl(STDIN, F_GETFL, 0) or die "F_GETFL";
$flags = fcntl(STDIN, F_SETFL, $flags | O_NONBLOCK) or die "F_SETFL";

$SIG{'PIPE'} = sub {};

vec($set, fileno(STDIN), 1) = 1;
vec($set, fileno($s), 1) = 1;

while (1) {
  (($n,$t) = select($rrdy=$set, undef, $err=$set, 1));
  if ($n) {
    if (vec($err, fileno(STDIN), 1)) {
      exit 0;
    }
    if (vec($err, fileno($s), 1)) {
      die "lost connection\n";
    }

    if (vec($rrdy, fileno(STDIN), 1)) {
      $st = sysread STDIN, $in, 1024;
      exit 0 if $st == 0 || $st eq '';
      $s->send($in);
    }
    if (vec($rrdy, fileno($s), 1)) {
      $st = sysread $s, $in, 1024;
      die "lost connection\n" if $st == 0 || $st eq '';
      print "rx data $st: '$in'\n";
    }
  }
}

