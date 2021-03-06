######################################################################
# Copyright (C) 1999, International Business Machines
# Corporation and others.  All Rights Reserved.
######################################################################
# See: ftp://elsie.nci.nih.gov/pub/tzdata<year>
# where <year> is "1999b" or a similar string.
######################################################################
# This package handles the parsing of time zone files.
# Author: Alan Liu
######################################################################
# Usage:
# Call ParseFile for each file to be imported.  Then call Postprocess
# to remove unused rules and links.

package TZ;
use strict;
use Carp;
use vars qw(@ISA @EXPORT $VERSION $YEAR $STANDARD);
require 'dumpvar.pl';

@ISA = qw(Exporter);
@EXPORT = qw(ParseFile
             Postprocess
             );
$VERSION = '0.1';

$STANDARD = '-'; # Name of the Standard Time rule

######################################################################
# Param: File name
# Param: Ref to hash of zones
# Param: Ref to hash of rules
# Param: Current year
sub ParseFile {
    my ($FILE, $ZONES, $RULES, $YEAR) = @_;

    local(*FILE);
    open(FILE,"<$FILE") or confess "Can't open $FILE: $!";
    my $zone; # Current zone
    my $badLineCount = 0;
    while (<FILE>) {
        s/\#.*//;
        next if (!/\S/);
        #|# Zone NAME           GMTOFF  RULES   FORMAT  [UNTIL]
        #|Zone America/Montreal -4:54:16 -      LMT     1884
        #|                      -5:00   Mont    E%sT
        #|Zone America/Thunder_Bay -5:57:00 -   LMT     1895
        #|                      -5:00   Canada  E%sT    1970
        #|                      -5:00   Mont    E%sT    1973
        #|                      -5:00   -       EST     1974
        #|                      -5:00   Canada  E%sT
        my ($zoneGmtoff, $zoneRule, $zoneFormat, $zoneUntil);
        if (/^zone/i) {
            # Zone block start
            if (/^zone\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/i
                || /^zone\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)()/i) {
                $zone = $1;
                ($zoneGmtoff, $zoneRule, $zoneFormat, $zoneUntil) =
                    ($2, $3, $4, $5);
            } else {
                print STDERR "Can't parse in $FILE: $_";
                ++$badLineCount;
            }
        } elsif (/^\s/ && $zone) {
            # Zone continuation
            if (/^\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/
                || /^\s+(\S+)\s+(\S+)\s+(\S+)()/) {
                ($zoneGmtoff, $zoneRule, $zoneFormat, $zoneUntil) =
                    ($1, $2, $3, $4);
            } else {
                print STDERR "Can't parse in $FILE: $_";
                ++$badLineCount;
            }
        } elsif (/^rule/i) {
            # Here is where we parse a single line of the rule table.
            # Our goal is to accept only rules applying to the current
            # year.  This is normally a matter of accepting rules
            # that match the current year.  However, in some cases this
            # is more complicated.  For example:
            #|# Tonga
            #|# Rule NAME FROM TO  TYPE IN  ON      AT    SAVE LETTER/S
            #|Rule  Tonga 1999 max -    Oct Sat>=1  2:00s 1:00 S
            #|Rule  Tonga 2000 max -    Apr Sun>=16 2:00s 0    -
            # To handle this properly, we save every rule we encounter
            # (thus overwriting older ones with newer ones, since rules
            # are listed in order), and also use slot [2] to mark when
            # we see a current year rule.  When that happens, we stop
            # saving rules.  Thus we match the latest rule we see, or
            # a matching rule if we find one.  The format of slot [2]
            # is just a 2 bit flag ([2]&1 means slot [0] matched,
            # [2]&2 means slot [1] matched).

            # Note that later, when the rules are post processed
            # (see Postprocess), the slot [2] will be overwritten
            # with the compressed rule string used to implement
            # equality testing.

            $zone = undef;
            # Rule
            #|# Rule NAME FROM TO   TYPE IN  ON      AT   SAVE LETTER/S
            #|Rule   US   1918 1919 -    Mar lastSun 2:00 1:00 W # War
            #|Rule   US   1918 1919 -    Oct lastSun 2:00 0    S
            #|Rule   US   1942 only -    Feb 9       2:00 1:00 W # War
            #|Rule   US   1945 only -    Sep 30      2:00 0    S
            #|Rule   US   1967 max  -    Oct lastSun 2:00 0    S
            #|Rule   US   1967 1973 -    Apr lastSun 2:00 1:00 D
            #|Rule   US   1974 only -    Jan 6       2:00 1:00 D
            #|Rule   US   1975 only -    Feb 23      2:00 1:00 D
            #|Rule   US   1976 1986 -    Apr lastSun 2:00 1:00 D
            #|Rule   US   1987 max  -    Apr Sun>=1  2:00 1:00 D
            if (/^rule\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+
                (\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/xi) {
                my ($name, $from, $to, $type, $in, $on, $at, $save, $letter) =
                    ($1, $2, $3, $4, $5, $6, $7, $8, $9);
                my $i = $save ? 0:1;

                if (!exists $RULES->{$name}) {
                    $RULES->{$name} = [];
                }
                my $ruleArray = $RULES->{$name};

                # Check our bit mask to see if we've already matched
                # a current rule.  If so, do nothing.  If not, then
                # save this rule line as the best one so far.
                if (@{$ruleArray} < 3 ||
                    !($ruleArray->[2] & $i)) {
                    my $h = $ruleArray->[$i];
                    $ruleArray->[$i]->{from} = $from;
                    $ruleArray->[$i]->{to} = $to;
                    $ruleArray->[$i]->{type} = $type;
                    $ruleArray->[$i]->{in} = $in;
                    $ruleArray->[$i]->{on} = $on;
                    $ruleArray->[$i]->{at} = $at;
                    $ruleArray->[$i]->{save} = $save;
                    $ruleArray->[$i]->{letter} = $letter;

                    # Does this rule match the current year?  If so,
                    # set the bit mask so we don't overwrite this rule.
                    # This makes us ingore rules for subsequent years
                    # that are already listed in the database -- as long
                    # as we have an overriding rule for the current year.
                    if (($from == $YEAR && $to =~ /only/i) ||
                        ($from <= $YEAR &&
                         (($to =~ /^\d/ && $YEAR <= $to) || $to =~ /max/i))) {
                        $ruleArray->[2] |= $i;
                    }
                }
            } else {
                print STDERR "Can't parse in $FILE: $_";
                ++$badLineCount;
            }
        } elsif (/^link/i) {
            #|# Old names, for S5 users
            #|
            #|# Link    LINK-FROM               LINK-TO
            #|Link      America/New_York        EST5EDT
            #|Link      America/Chicago         CST6CDT
            #|Link      America/Denver          MST7MDT
            #|Link      America/Los_Angeles     PST8PDT
            #|Link      America/Indianapolis    EST
            #|Link      America/Phoenix         MST
            #|Link      Pacific/Honolulu        HST
            if (/^link\s+(\S+)\s+(\S+)/i) {
                # We currently only record a single link -- if there
                # are more than one, we should modify this.
                my ($from, $to) = ($1, $2);
                $ZONES->{$from}->{link} = $to;
            } else {
                print STDERR "Can't parse in $FILE: $_";
                ++$badLineCount;
            }
        } else {
            # Unexpected line
            print STDERR "Ignoring in $FILE: $_";
            ++$badLineCount;
        }
        if ($zoneRule &&
            ($zoneUntil !~ /\S/ || ($zoneUntil =~ /^\d/ &&
                                    $zoneUntil >= $YEAR))) {
            $ZONES->{$zone}->{gmtoff} = $zoneGmtoff;
            $ZONES->{$zone}->{rule} = $zoneRule;
            $ZONES->{$zone}->{format} = $zoneFormat;
            $ZONES->{$zone}->{until} = $zoneUntil;
        }
    }
    close(FILE);
}

######################################################################
# Param: Ref to hash of zones
# Param: Ref to hash of rules
sub Postprocess {
    my ($ZONES, $RULES) = @_;
    my %ruleInUse;

    # Eliminate zone links that have no corresponding zone
    foreach (keys %$ZONES) {
        if (exists $ZONES->{$_}->{link} && !exists $ZONES->{$_}->{rule}) {
            if (0) {
                print STDERR
                    "Deleting link from historical/nonexistent zone: ",
                    $_, " -> ", $ZONES->{$_}->{link}, "\n";
            }
            delete $ZONES->{$_};
        }
    }

    # Check that each zone has a corresponding rule
    foreach (sort keys %$ZONES) {
        my $ruleName = $ZONES->{$_}->{rule};
        next if ($ruleName eq $STANDARD);
        if (exists $RULES->{$ruleName}) {
            $ruleInUse{$ruleName} = 1;
        } else {
            # This means the zone is using the standard rule now
            $ZONES->{$_}->{rule} = $STANDARD;
        }
    }

    # Check that both parts are there for rules
    # Check for unused rules
    # Make coded string for comparisons
    foreach (keys %$RULES) {
        if (!exists $ruleInUse{$_}) {
            if (0) {
                print STDERR "Deleting historical/unused rule: $_\n";
            }
            delete $RULES->{$_};
        } elsif (!$RULES->{$_}->[0] || !$RULES->{$_}->[1]) {
            print STDERR "Rule doesn't have both parts: $_\n";
        } else {
            # Generate code
            # This has all the data about a rule; it can be used
            # to see if two rules behave identically
            $RULES->{$_}->[2] =
                lc($RULES->{$_}->[0]->{in} . "," .
                   $RULES->{$_}->[0]->{on} . "," .
                   $RULES->{$_}->[0]->{at} . "," .
                   $RULES->{$_}->[0]->{save} . ";" .
                   $RULES->{$_}->[1]->{in} . "," .
                   $RULES->{$_}->[1]->{on} . "," .
                   $RULES->{$_}->[1]->{at}); # [1]->{save} is always zero
        }
    }
}
