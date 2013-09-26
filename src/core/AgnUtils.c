#include <errno.h>
#include <string.h>
#include <time.h>
#include "AgnCanonGeneStream.h"
#include "AgnFilterStream.h"
#include "AgnGtExtensions.h"
#include "AgnGeneLocus.h"
#include "AgnInferCDSVisitor.h"
#include "AgnInferExonsVisitor.h"
#include "AgnUtils.h"

void agn_bron_kerbosch( GtArray *R, GtArray *P, GtArray *X, GtArray *cliques,
                        bool skipsimplecliques )
{
  gt_assert(R != NULL && P != NULL && X != NULL && cliques != NULL);

  if(gt_array_size(P) == 0 && gt_array_size(X) == 0)
  {
    if(skipsimplecliques == false || gt_array_size(R) != 1)
    {
      GtUword i;
      AgnTranscriptClique *clique = agn_transcript_clique_new();
      for(i = 0; i < gt_array_size(R); i++)
      {
        GtFeatureNode *transcript = *(GtFeatureNode **)gt_array_get(R, i);
        agn_transcript_clique_add(clique, transcript);
      }
      gt_array_add(cliques, clique);
    }
  }

  while(gt_array_size(P) > 0)
  {
    GtGenomeNode *v = *(GtGenomeNode **)gt_array_get(P, 0);

    // newR = R \union {v}
    GtArray *newR = agn_gt_array_copy(R, sizeof(GtGenomeNode *));
    gt_array_add(newR, v);
    // newP = P \intersect N(v)
    GtArray *newP = agn_feature_neighbors(v, P);
    // newX = X \intersect N(v)
    GtArray *newX = agn_feature_neighbors(v, X);

    // Recursive call
    // agn_bron_kerbosch(R \union {v}, P \intersect N(v), X \intersect N(X))
    agn_bron_kerbosch(newR, newP, newX, cliques, skipsimplecliques);

    // Delete temporary arrays just created
    gt_array_delete(newR);
    gt_array_delete(newP);
    gt_array_delete(newX);

    // P := P \ {v}
    gt_array_rem(P, 0);

    // X := X \union {v}
    gt_array_add(X, v);
  }
}

double agn_calc_edit_distance(GtFeatureNode *t1, GtFeatureNode *t2)
{
  AgnTranscriptClique *clique1 = agn_transcript_clique_new();
  agn_transcript_clique_add(clique1, t1);
  AgnTranscriptClique *clique2 = agn_transcript_clique_new();
  agn_transcript_clique_add(clique2, t2);

  GtGenomeNode *gn1 = (GtGenomeNode *)t1;
  GtRange r1 = gt_genome_node_get_range(gn1);
  GtStr *seqid = gt_genome_node_get_seqid(gn1);
  GtGenomeNode *gn2 = (GtGenomeNode *)t2;
  GtRange r2 = gt_genome_node_get_range(gn2);
  GtRange local_range = r1;
  if(r2.start < r1.start)
    local_range.start = r2.start;
  if(r2.end > r1.end)
    local_range.end = r2.end;

  AgnCliquePair *pair = agn_clique_pair_new(gt_str_get(seqid), clique1, clique2,
                                            &local_range);
  agn_clique_pair_build_model_vectors(pair);
  agn_clique_pair_comparative_analysis(pair);

  double ed = agn_clique_pair_get_edit_distance(pair);

  agn_transcript_clique_delete(clique1);
  agn_transcript_clique_delete(clique2);
  agn_clique_pair_delete(pair);

  return ed;
}

double agn_calc_splice_complexity(GtArray *transcripts)
{
  GtUword n = gt_array_size(transcripts);
  GtUword i,j;
  double sc = 0.0;

  for(i = 0; i < n; i++)
  {
    GtFeatureNode *t_i = *(GtFeatureNode **)gt_array_get(transcripts, i);
    for(j = 0; j < i; j++)
    {
      GtFeatureNode *t_j = *(GtFeatureNode **)gt_array_get(transcripts, j);
      if(agn_gt_feature_node_overlap(t_i, t_j))
      {
        sc += agn_calc_edit_distance(t_i, t_j);
      }
    }
  }

  return sc;
}

GtFeatureNode *agn_eden()
{
  GtGenomeNode *gene, *mrna1, *mrna2, *mrna3, *feature;
  GtFeatureNode *genefn, *mrna1fn, *mrna2fn, *mrna3fn, *featurefn;
  GtGenomeNode *f1, *f2, *f3, *f4;
  GtFeatureNode *fn1, *fn2, *fn3, *fn4;
  GtStr *seqid = gt_str_new_cstr("ctg123");

  gene = gt_feature_node_new(seqid, "gene", 1000, 9000, GT_STRAND_FORWARD);
  genefn = (GtFeatureNode *)gene;
  gt_feature_node_add_attribute(genefn, "ID", "EDEN");

  mrna1 = gt_feature_node_new(seqid, "mRNA", 1050, 9000, GT_STRAND_FORWARD);
  mrna1fn = (GtFeatureNode *)mrna1;
  gt_feature_node_add_attribute(mrna1fn, "ID", "EDEN.1");
  gt_feature_node_add_child(genefn, mrna1fn);
  mrna2 = gt_feature_node_new(seqid, "mRNA", 1050, 9000, GT_STRAND_FORWARD);
  mrna2fn = (GtFeatureNode *)mrna2;
  gt_feature_node_add_attribute(mrna2fn, "ID", "EDEN.2");
  gt_feature_node_add_child(genefn, mrna2fn);
  mrna3 = gt_feature_node_new(seqid, "mRNA", 1300, 9000, GT_STRAND_FORWARD);
  mrna3fn = (GtFeatureNode *)mrna3;
  gt_feature_node_add_attribute(mrna3fn, "ID", "EDEN.3");
  gt_feature_node_add_child(genefn, mrna3fn);

  feature = gt_feature_node_new(seqid, "exon", 1050, 1500, GT_STRAND_FORWARD);
  featurefn = (GtFeatureNode *)feature;
  gt_feature_node_add_child(mrna1fn, featurefn);
  gt_feature_node_add_child(mrna2fn, featurefn);
  feature = gt_feature_node_new(seqid, "exon", 1300, 1500, GT_STRAND_FORWARD);
  featurefn = (GtFeatureNode *)feature;
  gt_feature_node_add_child(mrna3fn, featurefn);
  feature = gt_feature_node_new(seqid, "exon", 3000, 3902, GT_STRAND_FORWARD);
  featurefn = (GtFeatureNode *)feature;
  gt_feature_node_add_child(mrna1fn, featurefn);
  gt_feature_node_add_child(mrna3fn, featurefn);
  feature = gt_feature_node_new(seqid, "exon", 5000, 5500, GT_STRAND_FORWARD);
  featurefn = (GtFeatureNode *)feature;
  gt_feature_node_add_child(mrna1fn, featurefn);
  gt_feature_node_add_child(mrna2fn, featurefn);
  gt_feature_node_add_child(mrna3fn, featurefn);
  feature = gt_feature_node_new(seqid, "exon", 7000, 9000, GT_STRAND_FORWARD);
  featurefn = (GtFeatureNode *)feature;
  gt_feature_node_add_child(mrna1fn, featurefn);
  gt_feature_node_add_child(mrna2fn, featurefn);
  gt_feature_node_add_child(mrna3fn, featurefn);

  f1 = gt_feature_node_new(seqid, "CDS", 1201, 1500, GT_STRAND_FORWARD);
  fn1 = (GtFeatureNode *)f1;
  gt_feature_node_make_multi_representative(fn1);
  gt_feature_node_set_multi_representative(fn1, fn1);
  gt_feature_node_add_child(mrna1fn, fn1);
  f2 = gt_feature_node_new(seqid, "CDS", 3000, 3902, GT_STRAND_FORWARD);
  fn2 = (GtFeatureNode *)f2;
  gt_feature_node_make_multi_representative(fn2);
  gt_feature_node_set_multi_representative(fn2, fn1);
  gt_feature_node_add_child(mrna1fn, fn2);
  f3 = gt_feature_node_new(seqid, "CDS", 5000, 5500, GT_STRAND_FORWARD);
  fn3 = (GtFeatureNode *)f3;
  gt_feature_node_make_multi_representative(fn3);
  gt_feature_node_set_multi_representative(fn3, fn1);
  gt_feature_node_add_child(mrna1fn, fn3);
  f4 = gt_feature_node_new(seqid, "CDS", 7000, 7600, GT_STRAND_FORWARD);
  fn4 = (GtFeatureNode *)f4;
  gt_feature_node_make_multi_representative(fn4);
  gt_feature_node_set_multi_representative(fn4, fn1);
  gt_feature_node_add_child(mrna1fn, fn4);

  f1 = gt_feature_node_new(seqid, "CDS", 1201, 1500, GT_STRAND_FORWARD);
  fn1 = (GtFeatureNode *)f1;
  gt_feature_node_make_multi_representative(fn1);
  gt_feature_node_set_multi_representative(fn1, fn1);
  gt_feature_node_add_child(mrna2fn, fn1);
  f2 = gt_feature_node_new(seqid, "CDS", 5000, 5500, GT_STRAND_FORWARD);
  fn2 = (GtFeatureNode *)f2;
  gt_feature_node_make_multi_representative(fn2);
  gt_feature_node_set_multi_representative(fn2, fn1);
  gt_feature_node_add_child(mrna2fn, fn2);
  f3 = gt_feature_node_new(seqid, "CDS", 7000, 7600, GT_STRAND_FORWARD);
  fn3 = (GtFeatureNode *)f3;
  gt_feature_node_make_multi_representative(fn3);
  gt_feature_node_set_multi_representative(fn3, fn1);
  gt_feature_node_add_child(mrna2fn, fn3);

  f1 = gt_feature_node_new(seqid, "CDS", 3301, 3902, GT_STRAND_FORWARD);
  fn1 = (GtFeatureNode *)f1;
  gt_feature_node_make_multi_representative(fn1);
  gt_feature_node_set_multi_representative(fn1, fn1);
  gt_feature_node_add_child(mrna3fn, fn1);
  f2 = gt_feature_node_new(seqid, "CDS", 5000, 5500, GT_STRAND_FORWARD);
  fn2 = (GtFeatureNode *)f2;
  gt_feature_node_make_multi_representative(fn2);
  gt_feature_node_set_multi_representative(fn2, fn1);
  gt_feature_node_add_child(mrna3fn, fn2);
  f3 = gt_feature_node_new(seqid, "CDS", 7000, 7600, GT_STRAND_FORWARD);
  fn3 = (GtFeatureNode *)f3;
  gt_feature_node_make_multi_representative(fn3);
  gt_feature_node_set_multi_representative(fn3, fn1);
  gt_feature_node_add_child(mrna3fn, fn3);

  gt_str_delete(seqid);
  return genefn;
}

GtArray* agn_enumerate_feature_cliques(GtArray *feature_set)
{
  GtArray *cliques = gt_array_new( sizeof(GtArray *) );

  if(gt_array_size(feature_set) == 1)
  {
    GtFeatureNode *fn = *(GtFeatureNode **)gt_array_get(feature_set, 0);
    AgnTranscriptClique *clique = agn_transcript_clique_new();
    agn_transcript_clique_add(clique, fn);
    gt_array_add(cliques, clique);
  }
  else
  {
    // First add each transcript as a clique, even if it is not a maximal clique
    GtUword i;
    for(i = 0; i < gt_array_size(feature_set); i++)
    {
      GtFeatureNode *fn = *(GtFeatureNode **)gt_array_get(feature_set, i);
      AgnTranscriptClique *clique = agn_transcript_clique_new();
      agn_transcript_clique_add(clique, fn);
      gt_array_add(cliques, clique);
    }

    // Then use the Bron-Kerbosch algorithm to find all maximal cliques
    // containing >1 transcript
    GtArray *R = gt_array_new( sizeof(GtGenomeNode *) );
    GtArray *P = agn_gt_array_copy(feature_set, sizeof(GtGenomeNode *));
    GtArray *X = gt_array_new( sizeof(GtGenomeNode *) );

    // Initial call: agn_bron_kerbosch(\emptyset, vertex_set, \emptyset )
    agn_bron_kerbosch(R, P, X, cliques, true);

    gt_array_delete(R);
    gt_array_delete(P);
    gt_array_delete(X);
  }

  return cliques;
}

GtArray* agn_feature_neighbors(GtGenomeNode *feature, GtArray *feature_set)
{
  GtArray *neighbors = gt_array_new( sizeof(GtGenomeNode *) );
  GtUword i;
  for(i = 0; i < gt_array_size(feature_set); i++)
  {
    GtGenomeNode *other = *(GtGenomeNode **)gt_array_get(feature_set, i);
    if(other != feature)
    {
      GtRange feature_range = gt_genome_node_get_range(feature);
      GtRange other_range = gt_genome_node_get_range(other);
      if(gt_range_overlap(&feature_range, &other_range) == false)
        gt_array_add(neighbors, other);
    }
  }
  return neighbors;
}

FILE *agn_fopen(const char *filename, const char *mode, FILE *errstream)
{
  FILE *fp = fopen(filename, mode);
  if(fp == NULL)
  {
    fprintf(errstream, "error: could not open '%s' (%s)\n", filename,
            strerror(errno));
    exit(1);
  }
  return fp;
}

GtFeatureIndex *agn_import_canonical(int numfiles, const char **filenames,
                                     AgnLogger *logger)
{
  GtNodeStream *gff3 = gt_gff3_in_stream_new_unsorted(numfiles, filenames);
  gt_gff3_in_stream_check_id_attributes((GtGFF3InStream *)gff3);
  gt_gff3_in_stream_enable_tidy_mode((GtGFF3InStream *)gff3);

  GtFeatureIndex *features = gt_feature_index_memory_new();
  GtNodeStream *cgstream = agn_canon_gene_stream_new(gff3, logger);
  GtNodeStream *featstream = gt_feature_in_stream_new(cgstream, features);

  GtError *error = gt_error_new();
  int result = gt_node_stream_pull(featstream, error);
  if(result == -1)
  {
    agn_logger_log_error(logger, "error processing node stream: %s",
                         gt_error_get(error));
  }
  gt_error_delete(error);

  if(agn_logger_has_error(logger))
  {
    gt_feature_index_delete(features);
    features = NULL;
  }
  gt_node_stream_delete(gff3);
  gt_node_stream_delete(cgstream);
  gt_node_stream_delete(featstream);
  return features;
}

GtFeatureIndex *agn_import_simple(int numfiles, const char **filenames,
                                  char *type, AgnLogger *logger)
{
  GtFeatureIndex *features = gt_feature_index_memory_new();

  GtNodeStream *gff3 = gt_gff3_in_stream_new_unsorted(numfiles, filenames);
  gt_gff3_in_stream_check_id_attributes((GtGFF3InStream *)gff3);
  gt_gff3_in_stream_enable_tidy_mode((GtGFF3InStream *)gff3);

  GtHashmap *typestokeep = gt_hashmap_new(GT_HASH_STRING, NULL, NULL);
  gt_hashmap_add(typestokeep, type, type);
  GtNodeStream *filterstream = agn_filter_stream_new(gff3, typestokeep);

  GtNodeStream *featstream = gt_feature_in_stream_new(filterstream, features);

  GtError *error = gt_error_new();
  int result = gt_node_stream_pull(featstream, error);
  if(result == -1)
  {
    agn_logger_log_error(logger, "error processing node stream: %s",
                         gt_error_get(error));
  }
  gt_error_delete(error);

  if(agn_logger_has_error(logger))
  {
    gt_feature_index_delete(features);
    features = NULL;
  }
  gt_node_stream_delete(gff3);
  gt_node_stream_delete(filterstream);
  gt_node_stream_delete(featstream);
  return features;
}

bool agn_infer_cds_range_from_exon_and_codons(GtRange *exon_range,
                                              GtRange *leftcodon_range,
                                              GtRange *rightcodon_range,
                                              GtRange *cds_range)
{
  cds_range->start = 0;
  cds_range->end   = 0;

  // UTR
  if(exon_range->end < leftcodon_range->start ||
     exon_range->start > rightcodon_range->end)
    return false;

  bool overlap_left  = gt_range_overlap(exon_range, leftcodon_range);
  bool overlap_right = gt_range_overlap(exon_range, rightcodon_range);
  if(overlap_left && overlap_right)
  {
    cds_range->start = leftcodon_range->start;
    cds_range->end   = rightcodon_range->end;
  }
  else if(overlap_left)
  {
    cds_range->start = leftcodon_range->start;
    cds_range->end   = exon_range->end;
  }
  else if(overlap_right)
  {
    cds_range->start = exon_range->start;
    cds_range->end   = rightcodon_range->end;
  }
  else
  {
    cds_range->start = exon_range->start;
    cds_range->end   = exon_range->end;
  }

  return true;
}

GtStrArray* agn_seq_intersection(GtFeatureIndex *refrfeats,
                                 GtFeatureIndex *predfeats, AgnLogger *logger)
{
  // Fetch seqids from reference and prediction annotations
  GtError *e = gt_error_new();
  GtStrArray *refrseqids = gt_feature_index_get_seqids(refrfeats, e);
  if(gt_error_is_set(e))
  {
    agn_logger_log_error(logger, "error fetching seqids for reference: %s",
                         gt_error_get(e));
    gt_error_unset(e);
  }
  GtStrArray *predseqids = gt_feature_index_get_seqids(predfeats, e);
  if(gt_error_is_set(e))
  {
    agn_logger_log_error(logger, "error fetching seqids for prediction: %s",
                         gt_error_get(e));
    gt_error_unset(e);
  }
  gt_error_delete(e);
  if(agn_logger_has_error(logger))
  {
    gt_str_array_delete(refrseqids);
    gt_str_array_delete(predseqids);
    return NULL;
  }
  GtStrArray *seqids = agn_gt_str_array_intersection(refrseqids, predseqids);

  // Print reference sequences with no prediction annotations
  GtUword i, j;
  for(i = 0; i < gt_str_array_size(refrseqids); i++)
  {
    const char *refrseq = gt_str_array_get(refrseqids, i);
    int matches = 0;
    for(j = 0; j < gt_str_array_size(seqids); j++)
    {
      const char *seq = gt_str_array_get(seqids, j);
      if(strcmp(refrseq, seq) == 0)
        matches++;
    }
    if(matches == 0)
    {
      agn_logger_log_warning(logger, "no prediction annotations found for "
                             "sequence '%s'", refrseq);
    }
  }

  // Print prediction sequences with no reference annotations
  for(i = 0; i < gt_str_array_size(predseqids); i++)
  {
    const char *predseq = gt_str_array_get(predseqids, i);
    int matches = 0;
    for(j = 0; j < gt_str_array_size(seqids); j++)
    {
      const char *seq = gt_str_array_get(seqids, j);
      if(strcmp(predseq, seq) == 0)
        matches++;
    }
    if(matches == 0)
    {
      agn_logger_log_warning(logger, "no reference annotations found for "
                             "sequence '%s'", predseq);
    }
  }

  if(gt_str_array_size(seqids) == 0)
  {
    agn_logger_log_error(logger, "no sequences in common between reference and "
                         "prediction");
  }

  gt_str_array_delete(refrseqids);
  gt_str_array_delete(predseqids);
  return seqids;
}

GtStrArray* agn_seq_union(GtFeatureIndex *refrfeats, GtFeatureIndex *predfeats,
                          AgnLogger *logger)
{
  // Fetch seqids from reference and prediction annotations
  GtError *e = gt_error_new();
  GtStrArray *refrseqids = gt_feature_index_get_seqids(refrfeats, e);
  if(gt_error_is_set(e))
  {
    agn_logger_log_error(logger, "error fetching seqids for reference: %s",
                         gt_error_get(e));
    gt_error_unset(e);
  }
  GtStrArray *predseqids = gt_feature_index_get_seqids(predfeats, e);
  if(gt_error_is_set(e))
  {
    agn_logger_log_error(logger, "error fetching seqids for prediction: %s",
                         gt_error_get(e));
    gt_error_unset(e);
  }
  gt_error_delete(e);
  if(agn_logger_has_error(logger))
  {
    gt_str_array_delete(refrseqids);
    gt_str_array_delete(predseqids);
    return NULL;
  }
  GtStrArray *seqids = agn_gt_str_array_union(refrseqids, predseqids);

  gt_str_array_delete(refrseqids);
  gt_str_array_delete(predseqids);
  return seqids;
}

int agn_sprintf_comma(GtUword n, char *buffer)
{
  if(n < 1000)
  {
    int spaces = sprintf(buffer, "%lu", n);
    buffer += spaces;
    return spaces;
  }
  int spaces = agn_sprintf_comma(n / 1000, buffer);
  buffer += spaces;
  sprintf(buffer, ",%03lu", n % 1000);
  return spaces + 4;
}

int agn_string_compare(const void *p1, const void *p2)
{
  const char *s1 = *(char **)p1;
  const char *s2 = *(char **)p2;
  return strcmp(s1, s2);
}

GtArray *agn_test_data_genes_codons()
{
  GtArray *genes = gt_array_new( sizeof(GtGenomeNode *) );
  GtStr *seqid = gt_str_new_cstr("chr8");

  GtGenomeNode *gene1  = gt_feature_node_new(seqid, "gene", 22057, 23119,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *mrna1  = gt_feature_node_new(seqid, "mRNA", 22057, 23119,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *exon1  = gt_feature_node_new(seqid, "exon", 22057, 22382,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *exon2  = gt_feature_node_new(seqid, "exon", 22497, 22550,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *exon3  = gt_feature_node_new(seqid, "exon", 22651, 23119,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *start1 = gt_feature_node_new(seqid, "start_codon", 22167, 22169,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *stop1  = gt_feature_node_new(seqid, "stop_codon", 23020, 23022,
                                             GT_STRAND_FORWARD);
  gt_feature_node_add_child((GtFeatureNode *)gene1, (GtFeatureNode *)mrna1);
  gt_feature_node_add_child((GtFeatureNode *)mrna1, (GtFeatureNode *)exon1);
  gt_feature_node_add_child((GtFeatureNode *)mrna1, (GtFeatureNode *)exon2);
  gt_feature_node_add_child((GtFeatureNode *)mrna1, (GtFeatureNode *)exon3);
  gt_feature_node_add_child((GtFeatureNode *)mrna1, (GtFeatureNode *)start1);
  gt_feature_node_add_child((GtFeatureNode *)mrna1, (GtFeatureNode *)stop1);
  gt_array_add(genes, gene1);

  GtGenomeNode *gene2  = gt_feature_node_new(seqid, "gene", 48012, 48984,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *mrna2  = gt_feature_node_new(seqid, "mRNA", 48012, 48984,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon4  = gt_feature_node_new(seqid, "exon", 48012, 48537,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon5  = gt_feature_node_new(seqid, "exon", 48637, 48766,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon6  = gt_feature_node_new(seqid, "exon", 48870, 48984,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *start2 = gt_feature_node_new(seqid, "start_codon", 48982, 48984,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *stop2  = gt_feature_node_new(seqid, "stop_codon", 48411, 48413,
                                             GT_STRAND_FORWARD);
  gt_feature_node_add_child((GtFeatureNode *)gene2, (GtFeatureNode *)mrna2);
  gt_feature_node_add_child((GtFeatureNode *)mrna2, (GtFeatureNode *)exon4);
  gt_feature_node_add_child((GtFeatureNode *)mrna2, (GtFeatureNode *)exon5);
  gt_feature_node_add_child((GtFeatureNode *)mrna2, (GtFeatureNode *)exon6);
  gt_feature_node_add_child((GtFeatureNode *)mrna2, (GtFeatureNode *)start2);
  gt_feature_node_add_child((GtFeatureNode *)mrna2, (GtFeatureNode *)stop2);
  gt_array_add(genes, gene2);

  GtGenomeNode *gene3  = gt_feature_node_new(seqid, "gene", 88551, 92176,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *mrna3  = gt_feature_node_new(seqid, "mRNA", 88551, 92176,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon7  = gt_feature_node_new(seqid, "exon", 88551, 89029,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon8  = gt_feature_node_new(seqid, "exon", 89265, 89549,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon9  = gt_feature_node_new(seqid, "exon", 90074, 90413,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon10 = gt_feature_node_new(seqid, "exon", 90728, 90833,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon11 = gt_feature_node_new(seqid, "exon", 91150, 91362,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *exon12 = gt_feature_node_new(seqid, "exon", 91810, 92176,
                                             GT_STRAND_REVERSE);
  GtGenomeNode *start3 = gt_feature_node_new(seqid, "start_codon", 91961, 91963,
                                             GT_STRAND_FORWARD);
  GtGenomeNode *stop3  = gt_feature_node_new(seqid, "stop_codon", 88892, 88894,
                                             GT_STRAND_FORWARD);
  gt_feature_node_add_child((GtFeatureNode *)gene3, (GtFeatureNode *)mrna3);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon7);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon8);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon9);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon10);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon11);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)exon12);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)start3);
  gt_feature_node_add_child((GtFeatureNode *)mrna3, (GtFeatureNode *)stop3);
  gt_array_add(genes, gene3);

  gt_str_delete(seqid);
  return genes;
}

GtRange agn_transcript_cds_range(GtFeatureNode *transcript)
{
  gt_assert(transcript);
  GtRange trange;
  trange.start = 0;
  trange.end = 0;

  GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(transcript);
  GtFeatureNode *current;
  for
  (
    current = gt_feature_node_iterator_next(iter);
    current != NULL;
    current = gt_feature_node_iterator_next(iter)
  )
  {
    if(agn_gt_feature_node_is_cds_feature(current))
    {
      GtRange crange = gt_genome_node_get_range((GtGenomeNode *)current);
      if(trange.start == 0 || crange.start < trange.start)
        trange.start = crange.start;
      if(trange.end == 0 || crange.end > trange.end)
        trange.end = crange.end;
    }
  }

  if(gt_feature_node_get_strand(transcript) == GT_STRAND_REVERSE)
  {
    GtUword temp = trange.start;
    trange.start = trange.end;
    trange.end = temp;
  }
  return trange;
}

void agn_transcript_structure_gbk(GtFeatureNode *transcript, FILE *outstream)
{
  gt_assert(transcript && outstream);

  GtArray *exons = gt_array_new( sizeof(GtFeatureNode *) );
  GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(transcript);
  GtFeatureNode *child;
  for
  (
    child = gt_feature_node_iterator_next(iter);
    child != NULL;
    child = gt_feature_node_iterator_next(iter)
  )
  {
    if(agn_gt_feature_node_is_exon_feature(child))
      gt_array_add(exons, child);
  }
  gt_feature_node_iterator_delete(iter);

  gt_assert(gt_array_size(exons) > 0);
  gt_array_sort(exons, (GtCompare)agn_gt_genome_node_compare);

  if(gt_feature_node_get_strand(transcript) == GT_STRAND_REVERSE)
    fputs("complement(", outstream);

  if(gt_array_size(exons) == 1)
  {
    GtGenomeNode *exon = *(GtGenomeNode **)gt_array_get(exons, 0);
    GtRange exonrange = gt_genome_node_get_range(exon);
    fprintf(outstream, "<%lu..>%lu", exonrange.start, exonrange.end);
  }
  else
  {
    fputs("join(", outstream);
    GtUword i;
    for(i = 0; i < gt_array_size(exons); i++)
    {
      GtGenomeNode *exon = *(GtGenomeNode **)gt_array_get(exons, i);
      GtRange exonrange = gt_genome_node_get_range(exon);

      if(i == 0)
        fprintf(outstream, "<%lu..%lu", exonrange.start, exonrange.end);
      else if(i+1 == gt_array_size(exons))
        fprintf(outstream, ",%lu..>%lu", exonrange.start, exonrange.end);
      else
        fprintf(outstream, ",%lu..%lu", exonrange.start, exonrange.end);
    }
    fputs(")", outstream);
  }

  if(gt_feature_node_get_strand(transcript) == GT_STRAND_REVERSE)
    fputs(")", outstream);
}
