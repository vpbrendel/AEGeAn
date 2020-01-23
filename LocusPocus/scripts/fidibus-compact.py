#!/usr/bin/env python
#
# -----------------------------------------------------------------------------
# Copyright (c) 2016   Indiana University
#
# This file is part of AEGeAn (http://github.com/BrendelGroup/AEGeAn) and is
# licensed under the ISC license: see LICENSE.
# -----------------------------------------------------------------------------

from __future__ import print_function
from __future__ import division
import argparse
import math
import pandas
import re
import sys
import LocusPocus


def cli():
    """Define the command-line interface of the program."""
    desc = 'Calculate measures of compactness for the specified genome(s).'
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('-v', '--version', action='version',
                        version='LocusPocus v%s' % LocusPocus.__version__)
    parser.add_argument('-c', '--cfgdir', default=None, metavar='DIR',
                        help='directory (or comma-separated list of '
                        'directories) from which to load user-supplied genome '
                        'configuration files')
    parser.add_argument('-w', '--workdir', metavar='WD', default='./species',
                        help='working directory for data files; default is '
                        '"./species"')
    parser.add_argument('-l', '--length', metavar='LEN', type=int,
                        default=1000000, help='minimum length threshold; '
                        'default is 1000000 (1Mb)')
    parser.add_argument('-i', '--iqnt', metavar='QNT', type=float,
                        default=None, help='filter long iiLoci at the '
                        'specified length quantile (0.0-1.0)')
    parser.add_argument('-m', '--mqnt', metavar='QNT', type=float,
                        default=None, help='filter short miLoci at the '
                        'specified length quantile (0.0-1.0)')
    parser.add_argument('-d', '--centroid', type=float, default=None,
                        metavar='F',
                        help='by default, phi/sigma values are reported for '
                        'each chromosome/scaffold of at least 1 Mb in length; '
                        'enabling this option will instead calculate the '
                        'average (centroid) over all such phi/sigma values; '
                        'specify a factor "F" for filtering outliers; if a '
                        'point has a distance of more than "F" times the '
                        'average distance from the centroid, it is discarded '
                        'as an outlier; after all outliers are discarded the '
                        'centroid is recomputed')
    parser.add_argument('-s', '--shuffled', action='store_true',
                        help='load input from shuffled iLocus data')
    parser.add_argument('species', nargs='+', help='species label(s)')
    return parser


def longseqs(gff3file, minlength=1000000):
    with open(gff3file, 'r') as gff3:
        for line in gff3:
            if not line.startswith('##sequence-region'):
                continue
            pattern = r'##sequence-region\s+(\S+)\s+(\d+)\s+(\d+)'
            seqreg = re.search(pattern, line)
            assert seqreg, line
            seqid = seqreg.group(1)
            length = int(seqreg.group(3))
            if length >= minlength:
                yield seqid, length


def thresholds(iloci, iqnt=0.95, mqnt=0.05):
    ithresh = None
    if iqnt:
        iiloci = iloci.loc[iloci.LocusClass == 'iiLocus']
        ithresh = int(iiloci['Length'].quantile(iqnt))
    mthresh = None
    if mqnt:
        gilocus_types = ['siLocus', 'ciLocus', 'niLocus', 'miLocus']
        giloci = iloci.loc[iloci.LocusClass.isin(gilocus_types)]
        mthresh = int(giloci['Length'].quantile(mqnt))
    return ithresh, mthresh


def seqlen(seqid, iloci, ithresh=None, mthresh=None):
    seqloci = iloci.loc[(iloci.SeqID == seqid) &
                        (iloci.LocusClass != 'fiLocus')]
    effsize = seqloci['EffectiveLength'].sum()
    if ithresh:
        longiiloci = seqloci.loc[(seqloci.LocusClass == 'iiLocus') &
                                 (seqloci.Length > ithresh)]
        effsize -= longiiloci['EffectiveLength'].sum()
    if mthresh:
        gilocus_types = ['siLocus', 'ciLocus', 'niLocus', 'miLocus']
        shortgiloci = seqloci.loc[(seqloci.LocusClass.isin(gilocus_types)) &
                                  (seqloci.Length < mthresh)]
        effsize -= shortgiloci['EffectiveLength'].sum()
    return effsize


def calc_phi(seqid, miloci, mthresh=None):
    # ... distinguishing unmerged iLoci (ui) among all genic iLoci (gi):
    gilocus_types = ['siLocus', 'ciLocus', 'niLocus', 'miLocus']
    uilocus_types = ['siLocus', 'ciLocus', 'niLocus']
    giloci = miloci.loc[(miloci.SeqID == seqid) &
                        (miloci.LocusClass.isin(gilocus_types))]
    uiloci = miloci.loc[(miloci.SeqID == seqid) &
                        (miloci.LocusClass.isin(uilocus_types))]
    if mthresh:
        giloci = giloci.loc[giloci.Length >= mthresh]
        uiloci = uiloci.loc[uiloci.Length >= mthresh]
    nbrmerged = len(giloci) - len(uiloci)
    return nbrmerged / len(giloci)


def calc_centroid(x, y, outlierfactor=2.25):
    cent_x = sum(x) / len(x)
    cent_y = sum(y) / len(y)

    distances = list()
    for xi, yi in zip(x, y):
        distance = math.sqrt((xi - cent_x)**2 + (yi - cent_y)**2)
        distances.append(distance)
    avg_distance = sum(distances) / len(distances)

    keep_x = list()
    keep_y = list()
    for xi, yi, di in zip(x, y, distances):
        if di > avg_distance * outlierfactor:
            continue
        keep_x.append(xi)
        keep_y.append(yi)

    final_cent_x = sum(keep_x) / len(keep_x)
    final_cent_y = sum(keep_y) / len(keep_y)
    return final_cent_x, final_cent_y


def main(args):
    print('Species', 'SeqID', 'Sigma', 'Phi', sep='\t')
    for species in args.species:
        dtype = 'loci.shuffled' if args.shuffled else 'loci'
        ilocustable = '{wd:s}/{spec:s}/{spec:s}.i{dt:s}.tsv'.format(
            wd=args.workdir, spec=species, dt=dtype,
        )
        milocustable = '{wd:s}/{spec:s}/{spec:s}.mi{dt:s}.tsv'.format(
            wd=args.workdir, spec=species, dt=dtype,
        )
        iloci = pandas.read_table(ilocustable)
        miloci = pandas.read_table(milocustable)
        ithresh, mthresh = thresholds(miloci, args.iqnt, args.mqnt)

        phis = list()
        sigmas = list()
        seqids = list()
        gff3file = '{wd:s}/{spec:s}/{spec:s}.gff3'.format(
            wd=args.workdir, spec=species
        )
        for seqid, length in longseqs(gff3file, args.length):
            length = seqlen(seqid, miloci, ithresh, mthresh)
            try:
                phi = calc_phi(seqid, miloci, mthresh)
            except ZeroDivisionError:
                # ... the exception occurs when sequence contains no giloci
                continue
            milocus_occ = miloci.loc[
                (miloci.SeqID == seqid) &
                (miloci.LocusClass == 'miLocus')
            ]['Length'].sum()
            sigma = milocus_occ / length
            phis.append(phi)
            sigmas.append(sigma)
            seqids.append(seqid)

        if args.centroid:
            if len(phis)*len(sigmas) > 0:
                phi, sigma = calc_centroid(phis, sigmas, args.centroid)
                print(species, 'Centroid', sigma, phi, sep='\t')
        else:
            for seqid, sigma, phi in zip(seqids, sigmas, phis):
                print(species, seqid, sigma, phi, sep='\t')


if __name__ == '__main__':
    main(args=cli().parse_args())
