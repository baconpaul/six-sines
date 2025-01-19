#!/usr/bin/perl


use File::Find;
use File::Basename;

find(
    {
        wanted => \&findfiles,
    },
    '/Users/paul/Desktop/KD Update Patches'
);

sub findfiles
{
    $f = $File::Find::name;
    $d = $File::Find::dir;
    if ($f =~ m/\.sxsnp$/)
    {
        $q = basename($f);
        $in = "$d/$q";
        $out = $in;
        $out =~ s:/Users/paul/Desktop/KD Update Patches:/Users/paul/dev/music/bacon-projects/integer-fm/resources/factory_patches:;
        $out =~ s:_new.sxsnp:.sxsnp:;

        print "$in -> $out\n";
        system("mv \"$in\" \"$out\"")
    }
}