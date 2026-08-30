#!/usr/bin/env perl
use strict;
use warnings;

sub u32 {
    my ($data, $offset) = @_;
    die "out-of-range u32 at $offset\n" if $offset < 0 ||
        $offset + 4 > length($data);
    return unpack('V', substr($data, $offset, 4));
}

sub symbol_for_hash {
    my ($data, $hash) = @_;
    my $needle = pack('V', $hash);
    my $offset = -1;
    my $best_name;
    my $best_offset = -1;

    while (($offset = index($data, $needle, $offset + 1)) >= 0) {
        my $tail = substr($data, $offset + 4, 128);
        if ($tail =~ /^([A-Za-z_][\x20-\x7e]{1,126})\x00/) {
            my $candidate = $1;

            # A hash also occurs in binary function-table entries and call
            # operands.  Adjacent bytes can accidentally look like a tiny
            # printable string (for example the SetZone|S table entry looked
            # like "91").  Prefer the longest identifier-shaped candidate,
            # which selects the actual hash/string dictionary record.
            if (!defined($best_name) ||
                length($candidate) > length($best_name)) {
                $best_name = $candidate;
                $best_offset = $offset;
            }
        }
    }
    return defined($best_name) ? ($best_name, $best_offset) : ('?', -1);
}

sub usage {
    die "usage: $0 FILE (--hash HEX | --function HEX)\n";
}

@ARGV == 3 or usage();
my ($path, $mode, $value_text) = @ARGV;
$value_text =~ /^(?:0x)?([0-9a-fA-F]{1,8})$/ or usage();
my $value = hex($1);

open my $handle, '<:raw', $path or die "open $path: $!\n";
local $/;
my $data = <$handle>;
close $handle;

length($data) >= 0x14 && substr($data, 0, 13) eq "Version:0004\x00"
    or die "unsupported GEX header\n";
my $count = u32($data, 0x10);
my $table = 0x14;
my $hash_slot_count = u32($data, 0x0c);
my $table_end = $table + $count * 16;

# Version 0004 stores a fixed-width function table, an eight-byte index
# header, and then hash_slot_count four-byte index entries before serialized
# bytecode.  Treating table_end as bytecode was the source of a dangerous
# raw/logical-offset mix-up during the Talos audit (0x17E64 versus the exact
# 0x27E6C bytecode base in SOLWORLDM.gex).
$hash_slot_count > 0 && $hash_slot_count <= 0x100000
    or die "invalid GEX hash-slot count\n";
my $code_base = $table_end + 8 + $hash_slot_count * 4;
$table_end <= length($data) && $code_base <= length($data)
    or die "truncated function table or bytecode index\n";

if ($mode eq '--hash') {
    my ($name, $symbol_offset) = symbol_for_hash($data, $value);
    printf "hash=0x%08x symbol=%s symbol_offset=%s\n",
        $value, $name, $symbol_offset < 0 ? '?' : sprintf('0x%x', $symbol_offset);
    exit 0;
}
$mode eq '--function' or usage();

my ($start, $length, $flags, $index);
for my $candidate (0 .. $count - 1) {
    my $entry = $table + $candidate * 16;
    my $candidate_hash = u32($data, $entry + 4);
    next if $candidate_hash != $value;
    $flags = u32($data, $entry);
    $start = u32($data, $entry + 8);
    $length = u32($data, $entry + 12) & 0x7fffffff;
    $index = $candidate;
    last;
}
defined($start) or die sprintf("function hash 0x%08x not found\n", $value);
my ($name, $symbol_offset) = symbol_for_hash($data, $value);
printf "function hash=0x%08x name=%s index=%u flags=0x%08x " .
       "logical_start=0x%x length=0x%x file_start=0x%x code_base=0x%x " .
       "hash_slots=0x%x\n",
    $value, $name, $index, $flags, $start, $length,
    $code_base + $start, $code_base, $hash_slot_count;

my $body_start = $code_base + $start;
my $body_end = $body_start + $length;
$body_end <= length($data) or die "truncated function body\n";
for (my $offset = $body_start; $offset + 5 <= $body_end; ++$offset) {
    my $opcode = ord(substr($data, $offset, 1));
    next if $opcode != 0x27 && $opcode != 0x29;
    my $call_hash = u32($data, $offset + 1);
    my ($call_name) = symbol_for_hash($data, $call_hash);
    printf "  file=0x%x logical=0x%x opcode=0x%02x hash=0x%08x name=%s\n",
        $offset, $offset - $code_base, $opcode, $call_hash, $call_name;
}
