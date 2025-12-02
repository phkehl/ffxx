#!/usr/bin/perl
# Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
# https://oinkzwurgl.org/projaeggd/ffxx/
#
# This program is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, either version 3 of the
# License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see <https://www.gnu.org/licenses/>.

use strict;
use warnings;
use utf8;
use FindBin;
use Path::Tiny;
use Data::Dumper;
use Digest::SHA;
use Clone;
use List::Util;

########################################################################################################################

my @STREAMS =
(
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'SERIAL', scheme => 'serial', label => 'Serial port or UART',
        spec => '<device>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]',
        mode => 'RW', conn_to => undef, inact_to => 0.0, retry_to => 5.0, hotplug => 0,
        docu => [ #                                                                                   ->|
            "<device> device path (e.g. '/dev/ttyUSB0'), <autobaud> is one of 'none' (default),",
            "'passive', 'ubx', 'fp' or 'auto', <baudrate> in [bps] (default: 115200 resp. 921600 for ACM",
            "devices, <mode> '8N1' (default, no other modes are currently supported), <flow> 'off'",
            "(default), 'sw' or 'hw'" ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        onlydoc => 1, type => 'TCPCLI_TCPCLIS', scheme => 'tcpcli(s)', label => 'TCP client (opt. TLS)',
        spec => '<host>:<port>',
        mode => 'RW', conn_to => 10.0, inact_to => 0.0, retry_to => 0.0, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number" ],
    },
    {
        onlydoc => 0, ref => 'TCPCLI_TCPCLIS', type => 'TCPCLI', scheme => 'tcpcli', label => 'TCP client (without TLS)',
    },
    {
        onlydoc => 0, ref => 'TCPCLI_TCPCLIS', type => 'TCPCLIS', scheme => 'tcpclis', label => 'TCP client (with TLS)',
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        onlydoc => 1, type => 'NTRIPCLI_NTRIPCLIS', scheme => 'ntripcli(s)', label => 'NTRIP client (opt. TLS)',
        spec => '[<credentials>@]<host>:<port>/<mountpoint>[:<version>]',
        spec => '<host>:<port>',
        mode => 'RW', conn_to => 10.0, inact_to => 10.0, retry_to => 5.0, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<credentials> is <username>:<password>, =<base64_encoded_credentials> or %<path> to read",
            "either from a file, <host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number",
            "<mountpoint> name of the caster mountpoint, <version> NTRIP version 'auto' (default), 'v1'",
            "or 'v2'" ],
    },
    {
        onlydoc => 0, ref => 'NTRIPCLI_NTRIPCLIS', type => 'NTRIPCLI', scheme => 'ntripcli', label => 'NTRIP client (without TLS)',
    },
    {
        onlydoc => 0, ref => 'NTRIPCLI_NTRIPCLIS', type => 'NTRIPCLIS', scheme => 'ntripcliS', label => 'NTRIP client (with TLS)',
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        onlydoc => 1, type => 'TELNET_TELNETS', scheme => 'telnet(s)', label => 'Telnet/RFC2217 client (opt. TLS)',
        spec => '<host>:<port>[:<baudrate>[:<autobaud>[:<mode>[:<flow>]]]]',
        mode => 'RW', conn_to => 10.0, inact_to => 10.0, retry_to => 5.0, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number, <baudrate> in [bps]",
            "(default: 115200), <autobaud> is one of 'none' (default), 'passive', 'ubx', 'fp' or 'auto',",
            "<mode> '8N1' (default, no other modes are currently supported), <flow> is 'off' (default),",
            "'sw' or 'hw'" ],
    },
    {
        onlydoc => 0, ref => 'TELNET_TELNETS', type => 'TELNET', scheme => 'telnet', label => 'TCP client (without TLS)',
    },
    {
        onlydoc => 0, ref => 'TELNET_TELNETS', type => 'TELNETS', scheme => 'telnets', label => 'TCP client (with TLS)',
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        onlydoc => 1, type => 'NTRIPSVR_NTRIPSVRS', scheme => 'ntripsvr(s)', label => 'NTRIP server (opt. TLS)',
        spec => '<credentials>@<host>:<port>/<mountpoint>[:<version>]',
        mode => 'WO', conn_to => 10.0, inact_to => undef, retry_to => 5.0, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<credentials> is <password> for v1, <username>:<password> for v2,",
            "=<base64_encoded_credentials> or %<path> to read either from a file, <host> address (<IPv4>",
            "or [<IPv6>]) or hostname, <port> port number <mountpoint> name of the caster mountpoint,",
            "<version> NTRIP version 'v1' (default) or 'v2'" ],
    },
    {
        onlydoc => 0, ref => 'NTRIPSVR_NTRIPSVRS', type => 'NTRIPSVR', scheme => 'ntripsvr', label => 'NTRIP server (without TLS)',
    },
    {
        onlydoc => 0, ref => 'NTRIPSVR_NTRIPSVRS', type => 'NTRIPSVRS', scheme => 'ntripsvrs', label => 'NTRIP server (with TLS)',
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'TCPSVR', scheme => 'tcpsvr', label => 'TCP server',
        spec => '[<host>]:<port>',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<host> address (<IPv4> or [<IPv6>]) or hostname (bind to all interfaces if empty),",
            "<port> port number. This stream accepts a maximum of 20 clients." ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'UDPCLI', scheme => 'udpcli', label => 'UDP client',
        spec => '<host>:<port>',
        mode => 'WO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<host> address (<IPv4> or [<IPv6>]) or hostname, <port> port number. This stream is not",
            "able to distinguish different clients (sources) and may mangle data if multiple clients",
            "send data at the same time." ],
    },
    {
        type => 'UDPSVR', scheme => 'udpsvr', label => 'UDP server',
        spec => '[<host>]:<port>',
        mode => 'RO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<host> address (<IPv4> or [<IPv6>]) or hostname (bind to all interfaces if empty),",
            "<port> port number." ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'SPIDEV', scheme => 'spidev', label => 'Linux spidev (master)',
        spec => '<device>[:<speed>[:<bpw>[:<xfersize>[:<mode>]]]]',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<device> device path (e.g. '/dev/spidev0.3'), <speed> [Hz] (default: 1000000), <bpw> 8, 16",
            "or 32 (default) bits per word, <xfersize> [bytes] (64-2048 and multiple of 4, default 64),",
            "<mode> SPI mode (flags from linux/spi/spi.h, default 0x00000000)",
            "This assumes that the device ignores all-0xff on input and sends all-0xff to indicate",
            "no data." ]
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'CANSTR', scheme => 'canstr', label => 'SocketCAN stream',
        spec => '<dev>:<canid_in>:<canid_out>[:<ff>[:<fd>[:<brs>]]]',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<dev> interface device (e.g. 'can0'), <canid_out> / <canid_in>  CAN ID for outgoing",
            "(write) / incoming (read) frames (0x001-0x7ff for SFF, 0x00000001-0x1fffffff for EFF),",
            "<ff> frame format ('sff' or 'eff'), <fd> 'fd' for CAN FD or '' for classical CAN,",
            "<brs> 'brs' or '' for CAN FD bitrate switch (only with <fd> = 'fd'). Note that CAN",
            "interface (bitrates etc.) must be configured appropriately, e.g. using 'ip link'."
            ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'GGA', scheme => 'gga', label => 'NMEA GGA generator',
        spec => '<lat>/<lon>/<height>[[:<interval>]:<talker>]',
        mode => 'RO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<lat> latitude [deg], <lon> longitude [deg], <height> height [m], <interval> output",
            "interval in [s] (1.0 - 86400.0 s, default: 5.0), <talker> NMEA talker ID (default 'GN')" ]
    },
    {
        type => 'STA', scheme => 'sta', label => 'RTCM3 station message generator',
        spec => '<x>/<y>/<z>[[[:<interval>]:<sta>]:<type>]',
        mode => 'RO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<x>/<y>/<z> ECEF coordinates [m], <interval> output interval in [s] (1.0 - 86400.0 s,",
            "default: 5.0), <sta> station ID (default 0), <type> message type (default 1005, 1006 or",
            "1032).",
            "DF022, DF023, DF024, DF142 are set to 1, DF021, DF141, DF364 and DF028 are set to 0" ]
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'LOOP', scheme => 'loop', label => 'Loopback (echo)',
        spec => '[<delay>][:<rate>]',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "Delay echoed data my <delay> [ms] (default 0) or limit rate of echoed data to",
            "<bytes_per_sec> bytes per second (0 to disable rate limiting, default 0)" ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'FILEOUT', scheme => 'fileout', label => 'File writer',
        spec => '<file>[:<swap>[:<ts>]]',
        mode => 'WO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<file> file path with optional placeholders for UTC  '\%Y' (year, e.g. 2024),",
            "'\%m' (month, 01-12) '\%d' (day, 01-31), '\%h' (hour, 00-23), '\%M' (minute, 00-59),",
            "'\%S' (second, 00-60), '\%j' (day of year, 001-366), '\%W' (GPS week number, e.g. 1234),",
            "'\%w' (day of GPS week, 0-6), '\%s' (GPS time of week [s], 0-604799), optional <swap> file",
            "swap time [s] (60-86400, negative value for unaligned timestamps, default: '', that is,",
            "no swap, <ts> store index sidecar file for replay ('ts') (default '', i.e. no sidecar file)" ],
    },
    {
        type => 'FILEIN', scheme => 'filein', label => 'File read',
        spec => '<file>[:<speed>[:<offset>]]',
        mode => 'RO', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<file> file path, <speed> replay speed (default 0.0, that is, ignore .ts file), <offset>",
            "replay offset [s] (default: 0.0)" ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'EXEC', scheme => 'exec', label => 'External program stdin/stdout',
        spec => '<path>[[:<arg>]...]',
        mode => 'RW', conn_to => undef, inact_to => 0.0, retry_to => 0.0, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<path> to executable, <arg> optional argument(s)" ],
    },
    # ------------------------------------------------------------------------------------------------------------------
    {
        type => 'IPCSVR', scheme => 'ipcsvr', label => 'Interprocess stream (server)',
        spec => '<name>',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => undef, hotplug => undef,
        docu => [ #                                                                                   ->|
            "<name> unique name for the connection" ],
    },
    {
        type => 'IPCCLI', scheme => 'ipccli', label => 'Interprocess stream (client)',
        spec => '<name>',
        mode => 'RW', conn_to => undef, inact_to => undef, retry_to => 5.0, hotplug => 1,
        docu => [ #                                                                                   ->|
            "<name> unique name for the connection" ],
    },
);

my %STREAM_DOCU =
(
    spec => [ #                       .                                                      ->|
        "The stream spec is in the form '<scheme>://<path>[,<option>][,<option>][...]'.",
        "The <scheme> defines the structure of the <path> and which <option>s are applicable.",
    ],
    opts => [ #                                                                                           ->|
        "The <option>s are (not all streams support all options):",
        "",
        "- N=<name>     -- A short and concise name for the stream ([a-zA-Z0-9_)]",
        "- RO           -- Make a RW stream read-only (input only), that is, ignore any writes (output)",
        "- WO           -- Make a RW stream write-only (output only), that is, ignore any reads (input)",
        "- C=<timeout>  -- Connect timeout [s] (1.0-3600.0, 0.0 to disable)",
        "- A=<timeout>  -- Read (and only read!) inactivity timeout [s] (1.0-3600.0, 0.0 to disable)",
        "- R=<timeout>  -- Retry timeout [s] (2.0-3600.0)",
        "- H=off|on     -- Initialise on start ('off') or allow delayed initialisation ('on'). Useful for",
        "                  hot-pluggable devices. Use with R=<timeout>.",
    ],
    tls => [ #                                                                                            ->|
        "Secure client streams (tcpclis://, etc.) can use TLS 1.2 or 1.3. To use server authentication the",
        "corresponding certificate must be available. They are loaded from the path or file given by the",
        "FFXX_STREAM_TLS_FILES_PATH environment variable. See the SSL_CTX_load_verify_locations(3ssl) man",
        "page for details. The certificate must match the used hostname or address. See X509_check_host(3ssl)",
        "man page for details.",
    ],
);

########################################################################################################################

my $FFXX_DIR = path("$FindBin::Bin")->canonpath();
my @CODEGEN_FILES = ();
my %CODEGEN_SECTIONS = ();

########################################################################################################################
# Generate code for streams
do
{
    foreach my $entry (@STREAMS) {
        if ($entry->{ref}) {
            my $ref = (grep { $_->{type} eq $entry->{ref} } @STREAMS)[0];
            $entry->{$_} = $ref->{$_} for (grep { !defined $entry->{$_} } keys %{$ref});
        }
    }

    push(@CODEGEN_FILES,
        path("$FFXX_DIR/stream.hpp"),
        path("$FFXX_DIR/stream.cpp"));
    $CODEGEN_SECTIONS{FFXX_STREAM_SPEC} = [];
    $CODEGEN_SECTIONS{FFXX_STREAM_OPTS} = [];
    $CODEGEN_SECTIONS{FFXX_STREAM_TLS} = [];
    $CODEGEN_SECTIONS{FFXX_STREAM_TYPES} = [];
    $CODEGEN_SECTIONS{FFXX_STREAM_HELP} = [];

    # Add <option>s docu
    foreach my $entry (@STREAMS)
    {
        next if ($entry->{ref});
        $entry = (grep { $_->{type} eq $entry->{ref} } @STREAMS)[0] if ($entry->{ref});

        my @options = ();
        if (defined $entry->{conn_to}) {
            push(@options, sprintf('C=<timeout> (%.1f)', $entry->{conn_to}));
        }
        if (defined $entry->{inact_to}) {
            push(@options, sprintf('A=<timeout> (%.1f)', $entry->{inact_to}));
        }
        if (defined $entry->{retry_to}) {
            push(@options, sprintf('R=<timeout> (%.1f)', $entry->{retry_to}));
        }
        if (defined $entry->{hotplug}) {
            push(@options, sprintf('H=off|on (%s)', $entry->{hotplug} ? 'on' : 'off'));
        }
        if ($#options >= 0) {
            push(@{$entry->{docu}}, "", "<option>s (default): " .join(", ", @options));
        }
    }

    sub help { my ($line) = @_; return $line ? "    \"$line\\n\"\n" : "    \"\\n\"\n"; }
    sub html { my ($line) = @_;
        $line =~ s{<}{&lt;}g;
        $line =~ s{>}{&gt;}g;
        $line =~ s{@}{@@}g;
        $line =~ s{(&lt;[^&]+&gt;)}{<code>$1</code>}g;
        return $line;
    }
    sub doxy { my ($line) = @_; return $line ? " * " . html($line) . "\n" : " *\n"; }

    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_SPEC}}, map { doxy($_) } @{$STREAM_DOCU{spec}});
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}}, map { help($_) } @{$STREAM_DOCU{spec}});


    # Summary
    my $s1 = List::Util::max(map { length($_->{scheme} || '') } @STREAMS);
    my $s2 = List::Util::max(map { length($_->{spec} || '') } @STREAMS);
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}},
        "    \"\\n\"\n",
        "    \"Summary:\\n\"\n",
        "    \"\\n\"\n");
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPES}},
        " *\n",
        " * \@subsubsection FFXX_STREAM_TYPES_SUMMARY Summary\n",
        " *\n");
    foreach my $entry (@STREAMS)
    {
        next if ($entry->{ref});
        my $line = sprintf("%-${s1}s  %-${s2}s  %-2s  %-6s  %-6s  %-6s  %s",
            $entry->{scheme}, $entry->{spec}, $entry->{mode},
            (defined $entry->{conn_to}  ? sprintf("C=%.1f", $entry->{conn_to})  : "-"),
            (defined $entry->{inact_to} ? sprintf("A=%.1f", $entry->{inact_to}) : "-"),
            (defined $entry->{retry_to} ? sprintf("R=%.1f", $entry->{retry_to}) : "-"),
            (defined $entry->{hotplug}  ? sprintf("H=%s", $entry->{hotplug} ? 'on' : 'off') : "-"));

        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}}, "    \"    $line\\n\"\n");
        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPES}}, " *     $line\n");
    }

    # Detailed help
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}},
        "    \"\\n\"\n",
        "    \"Details:\\n\"\n");
    foreach my $entry (@STREAMS)
    {
        next if ($entry->{ref});
        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPES}},
            " *\n",
            " * \@subsubsection FFXX_STREAM_TYPES_$entry->{type} $entry->{label} ($entry->{mode})\n",
            " *\n",
            " * Spec: <em>" . html("$entry->{scheme}://$entry->{spec}") . "</em>\n",
            " *\n",
            map { doxy($_) } @{$entry->{docu}});

        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}},
            "    \"\\n\"\n",
            "    \"    $entry->{scheme}://$entry->{spec} ($entry->{mode}) -- $entry->{label}\\n\"\n",
            "    \"\\n\"\n",
            map { help("        $_") } grep { $_ } @{$entry->{docu}});
    }

    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_OPTS}}, map { doxy($_); }     @{$STREAM_DOCU{opts}});
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}}, map { help($_); } "", @{$STREAM_DOCU{opts}});

    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TLS}},  map { doxy($_); }     @{$STREAM_DOCU{tls}});
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_HELP}}, map { help($_); } "", @{$STREAM_DOCU{tls}});


    # Generate enum class StreamType and StreamTypeStr()
    $CODEGEN_SECTIONS{FFXX_STREAM_TYPE_HPP} = [];
    $CODEGEN_SECTIONS{FFXX_STREAM_TYPE_CPP} = [];
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_HPP}}, "    UNSPECIFIED,          //!< Unspecified\n");
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_CPP}}, "        case StreamType::UNSPECIFIED: break;\n");
    foreach my $entry (@STREAMS)
    {
        next if ($entry->{onlydoc});
        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_HPP}},
            sprintf("    %-20s  //!< %s, see \@ref FFXX_STREAM_TYPES_%s\n",
                "$entry->{type},", $entry->{label}, $entry->{ref} || $entry->{type}));
        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_CPP}},
            sprintf("        case StreamType::%-20s return \"%s\";\n",
                "$entry->{type}:", $entry->{type}));
    }

    # Generate StreamTypeInfo
    $CODEGEN_SECTIONS{FFXX_STREAM_TYPE_INFO} = [];
    my $nStreams = scalar grep { !$_->{onlydoc} } @STREAMS;
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_INFO}},
        "static constexpr std::array<StreamTypeInfo, $nStreams> kStreamTypeInfos = {{\n");
    foreach my $entry (@STREAMS)
    {
        next if ($entry->{onlydoc});
        # printf("entry=%s\n", Dumper($entry));
        push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_INFO}},
            sprintf("    { %-30s %-15s %-20s %s, %5s, %5s, %3s, },\n",
                "StreamType::$entry->{type},", "\"$entry->{scheme}\",", "StreamMode::$entry->{mode},",
                (defined $entry->{conn_to}  ? sprintf("%.1f", $entry->{conn_to})  : "-1.0"),
                (defined $entry->{inact_to} ? sprintf("%.1f", $entry->{inact_to}) : "-1.0"),
                (defined $entry->{retry_to} ? sprintf("%.1f", $entry->{retry_to}) : "-1.0"),
                (defined $entry->{hotplug}  ? $entry->{hotplug}                   : "-1")));


    }
    push(@{$CODEGEN_SECTIONS{FFXX_STREAM_TYPE_INFO}}, "}};\n");
};


########################################################################################################################
# Update files as necessary

# printf("CODEGEN_FILES=%s\n", Dumper(\@CODEGEN_FILES));
# printf("CODEGEN_SECTIONS=%s\n", Dumper(\%CODEGEN_SECTIONS));

foreach my $file (@CODEGEN_FILES)
{
    my @lines = $file->lines();
    my $sha1 = Digest::SHA::sha1_hex(@lines);

    foreach my $section (sort keys %CODEGEN_SECTIONS)
    {
        my $in_block = 0;
        my @new_lines = ();
        foreach my $line (@lines)
        {
            if ($line =~ m{\@fp_codegen_end\{$section\}})
            {
                $in_block = 0;
            }
            push(@new_lines, $line) unless ($in_block);
            if ($line =~ m{\@fp_codegen_begin\{$section\}}) {
                $in_block = 1;
                push(@new_lines, @{$CODEGEN_SECTIONS{$section}});
            }
        }
        @lines = @new_lines;
    }
    my $new_sha1 = Digest::SHA::sha1_hex(@lines);

    if ($sha1 eq $new_sha1)
    {
        printf("up to date: %s\n", $file->relative($FFXX_DIR));
    }
    else
    {
        printf("updating:   %s\n", $file->relative($FFXX_DIR));
        $file->spew(@lines);
    }
}

########################################################################################################################
