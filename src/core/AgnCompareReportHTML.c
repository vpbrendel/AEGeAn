/**

Copyright (c) 2010-2014, Daniel S. Standage and CONTRIBUTORS

The AEGeAn Toolkit is distributed under the ISC License. See
the 'LICENSE' file in the AEGeAn source code distribution or
online at https://github.com/standage/AEGeAn/blob/master/LICENSE.

**/

#include <string.h>
#include "core/hashmap_api.h"
#include "AgnComparison.h"
#include "AgnCompareReportHTML.h"
#include "AgnLocus.h"
#include "AgnVersion.h"

#define compare_report_html_cast(GV)\
        gt_node_visitor_cast(compare_report_html_class(), GV)

//------------------------------------------------------------------------------
// Data structure definitions
//------------------------------------------------------------------------------

struct AgnCompareReportHTML
{
  const GtNodeVisitor parent_instance;
  AgnLocusPngMetadata *pngdata;
  AgnComparisonData data;
  GtStrArray *seqids;
  const char *outdir;
  GtHashmap *seqdata;
  GtHashmap *seqlocusdata;
  GtHashmap *compclassdata;
  AgnCompareReportHTMLOverviewFunc ofunc;
  void *ofuncdata;
  GtLogger *logger;
  GtStr *summary_title;
  bool gff3;
  GtUword locuscount;
};

typedef struct
{
  char     seqid[64];
  GtRange  lrange;
  unsigned numperfect, nummislabeled, numcdsmatch, numexonmatch, numutrmatch,
           numnonmatch;
  GtUword  refrtrans, predtrans;
} SeqfileLocusData;

//------------------------------------------------------------------------------
// Prototypes for private functions
//------------------------------------------------------------------------------

/**
 * @function Implement the GtNodeVisitor interface.
 */
static const GtNodeVisitorClass *compare_report_html_class();

/**
 * @function Print overall comparison statistics for a particular class of
 * feature comparisons in the summary report.
 */
static void compare_report_html_comp_class_summary(AgnCompClassDesc *summ,
                                                   GtUword num_comparisons,
                                                   const char *label,
                                                   const char *compkey,
                                                   const char *desc,
                                                   FILE *outstream);

/**
 * @function HTML footer for comparison reports.
 */
static void compare_report_html_footer(FILE *outstream);

/**
 * @function Free memory used by this node visitor.
 */
static void compare_report_html_free(GtNodeVisitor *nv);

/**
 * @function Create a report for each locus.
 */
static void compare_report_html_locus_handler(AgnCompareReportHTML *rpt,
                                              AgnLocus *locus);

/**
 * @function Print locus report header.
 */
static void compare_report_html_locus_header(AgnLocus *locus, FILE *outstream);

/**
 * @function Print gene IDs for locus report header.
 */
static void compare_report_html_locus_gene_ids(AgnLocus *locus,FILE *outstream);

/**
 * @function Print a report of nucleotide-level structure comparison for the
 * given clique pair.
 */
static void compare_report_html_pair_nucleotide(FILE *outstream,
                                                AgnCliquePair *pair);

/**
 * @function Print a report of feature-level structure comparison for the
 * given clique pair.
 */
static void compare_report_html_pair_structure(FILE *outstream,
                                               AgnCompStatsBinary *stats,
                                               const char *label,
                                               const char *units);

/**
 * @function List loci according to their comparison class: perfect match, CDS
 * match, etc.
 */
static void compare_report_html_print_compclassfiles(AgnCompareReportHTML *rpt);

/**
 * @function Add the locus' information to the sequence summary page
 */
static void
compare_report_html_print_locus_to_seqfile(SeqfileLocusData *data,
                                           bool printseqid, FILE *outstream);

/**
 * @function Print a comparison report for the given clique pair. Include markup
 * to collapse the comparison if required.
 */
static void compare_report_html_print_pair(AgnCliquePair *pair, FILE *outstream,
                                           GtUword k, bool collapse, bool gff3);

/**
 * @function Create the sequence-level summary pages.
 */
static void compare_report_html_print_seqfiles(AgnCompareReportHTML *rpt);

/**
 * @function Store informatino about each locus for later use when creating the
 * sequence-level summary pages.
 */
static void
compare_report_html_save_seq_locus_data(AgnCompareReportHTML *rpt,
                                        AgnLocus *locus);

/**
 * @function The HTML report includes a summary page for each sequence. This
 * function prints the header for that file.
 */
static void compare_report_html_seqfile_header(FILE *outstream,
                                               const char *seqid);

/**
 * @function The HTML report includes a summary page for each sequence. This
 * function prints the footer for that file.
 */
static void compare_report_html_seqfile_footer(FILE *outstream);

/**
 * @function Print an overview of reference and prediction annotations for the
 * summary report.
 */
static void compare_report_html_summary_annot(AgnCompInfo *info,
                                              FILE *outstream);

/**
 * @function Main body for summary report.
 */
static void compare_report_html_summary_body(AgnCompareReportHTML *rpt,
                                             FILE *outstream);

/**
 * @function Header material for summary report.
 */
static void compare_report_html_summary_headmatter(AgnCompareReportHTML *rpt,
                                                   FILE *outstream);

/**
 * @function Print a breakdown of characteristics of loci that fall into a
 * particular comparison class.
 */
static void compare_report_html_summary_struc(FILE *outstream,
                                              AgnCompStatsBinary *stats,
                                              const char *label,
                                              const char *units);

/**
 * @function Process feature nodes.
 */
static int compare_report_html_visit_feature_node(GtNodeVisitor *nv,
                                                  GtFeatureNode *fn,
                                                  GtError *error);

/**
 * @function Process region nodes.
 */
static int compare_report_html_visit_region_node(GtNodeVisitor *nv,
                                                 GtRegionNode *rn,
                                                 GtError *error);

//------------------------------------------------------------------------------
// Method implementations
//------------------------------------------------------------------------------

void agn_compare_report_html_create_summary(AgnCompareReportHTML *rpt)
{
  agn_assert(rpt);

  // Create the output file
  char filename[1024];
  sprintf(filename, "%s/index.html", rpt->outdir);
  FILE *outstream = fopen(filename, "w");
  if(outstream == NULL)
  {
    fprintf(stderr, "error: unable to open output file '%s'\n", filename);
    exit(1);
  }

  if(rpt->locuscount == 0)
  {
    fprintf(outstream, "<!doctype html>\n"
            "<html lang=\"en\">\n"
            "  <body><p>No loci for comparison.</p></body>\n"
            "</html>\n");
    fclose(outstream);
    return;
  }

  compare_report_html_summary_headmatter(rpt, outstream);
  compare_report_html_summary_body(rpt, outstream);
  compare_report_html_footer(outstream);

  fclose(outstream);
  compare_report_html_print_seqfiles(rpt);
  compare_report_html_print_compclassfiles(rpt);
}

GtNodeVisitor *agn_compare_report_html_new(const char *outdir, bool gff3,
                                           AgnLocusPngMetadata *pngdata,
                                           GtLogger *logger)
{
  GtNodeVisitor *nv = gt_node_visitor_create(compare_report_html_class());
  AgnCompareReportHTML *rpt = compare_report_html_cast(nv);
  rpt->pngdata = pngdata;
  agn_comparison_data_init(&rpt->data);
  rpt->seqids = gt_str_array_new();
  rpt->outdir = outdir;
  rpt->seqdata = gt_hashmap_new(GT_HASH_STRING, gt_free_func, gt_free_func);
  rpt->seqlocusdata = gt_hashmap_new(GT_HASH_STRING, gt_free_func,
                                     (GtFree)gt_array_delete);
  rpt->compclassdata = gt_hashmap_new(GT_HASH_STRING, gt_free_func,
                                      (GtFree)gt_array_delete);
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("perfect"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("mislabeled"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("cds"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("exon"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("utr"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  gt_hashmap_add(rpt->compclassdata, gt_cstr_dup("nonmatch"),
                 gt_array_new(sizeof(SeqfileLocusData)));
  rpt->logger = logger;
  rpt->summary_title = gt_str_new_cstr("ParsEval Summary");
  rpt->gff3 = gff3;
  rpt->locuscount = 0;

  return nv;
}

void agn_compare_report_html_reset_summary_title(AgnCompareReportHTML *rpt,
                                                 GtStr *title_string)
{
  gt_str_delete(rpt->summary_title);
  rpt->summary_title = gt_str_ref(title_string);
}

void agn_compare_report_html_set_overview_func(AgnCompareReportHTML *rpt,
                                         AgnCompareReportHTMLOverviewFunc func,
                                         void *funcdata)
{
  rpt->ofunc = func;
  rpt->ofuncdata = funcdata;
}

static const GtNodeVisitorClass *compare_report_html_class()
{
  static const GtNodeVisitorClass *nvc = NULL;
  if(!nvc)
  {
    nvc = gt_node_visitor_class_new(sizeof (AgnCompareReportHTML),
                                    compare_report_html_free, NULL,
                                    compare_report_html_visit_feature_node,
                                    compare_report_html_visit_region_node,
                                    NULL, NULL);
  }
  return nvc;
}


static void compare_report_html_compclass_header(FILE *outstream,
                                                 const char *compclass)
{
  fprintf(outstream,
          "<!doctype html>\n"
          "<html lang=\"en\">\n"
          "  <head>\n"
          "    <meta charset=\"utf-8\" />\n"
          "    <title>ParsEval: Loci containing %s</title>\n"
          "    <link rel=\"stylesheet\" type=\"text/css\" href=\"parseval.css\" />\n"
          "    <script type=\"text/javascript\" language=\"javascript\" src=\"vendor/jquery.js\"></script>\n"
          "    <script type=\"text/javascript\" language=\"javascript\" src=\"vendor/jquery.dataTables.js\"></script>\n"
          "    <script type=\"text/javascript\">\n"
          "      $(document).ready(function() {\n"
          "        $('#locus_table').dataTable( {\n"
          "          \"sScrollY\": \"400px\",\n"
          "          \"bPaginate\": false,\n"
          "          \"bScrollCollapse\": true,\n"
          "          \"bSort\": false,\n"
          "          \"bFilter\": false,\n"
          "          \"bInfo\": false\n"
          "        });\n"
          "      } );\n"
          "    </script>\n"
          "  </head>\n"
          "  <body>\n"
          "    <div id=\"content\">\n"
          "      <h1>Loci containing %s</h1>\n"
          "      <p><a href=\"index.html\">⇐ Back to summary</a></p>\n\n"
          "      <p class=\"indent\">\n"
          "        Below is a list of all loci containing %s.\n"
          "        Click on the <a>(+)</a> symbol for a report of the complete comparative analysis corresponding to each locus.\n"
          "      </p>\n\n"
          "      <table class=\"loci\" id=\"locus_table\">\n"
          "        <thead>\n"
          "          <tr>\n"
          "            <th>&nbsp;</th>\n"
          "            <th>Sequence</th>\n"
          "            <th>Start</th>\n"
          "            <th>End</th>\n"
          "            <th>Length</th>\n"
          "            <th>#Trans</th>\n"
          "            <th>Comparisons</th>\n"
          "          </tr>\n"
          "        </thead>\n"
          "        <tbody>\n",
          compclass, compclass, compclass);
}

static void compare_report_html_comp_class_summary(AgnCompClassDesc *summ,
                                                   GtUword num_comparisons,
                                                   const char *label,
                                                   const char *compkey,
                                                   const char *desc,
                                                   FILE *outstream)
{
  GtUword numclass     = summ->comparison_count;
  float perc_class     = 100.0 * (float)numclass / (float)num_comparisons;
  float mean_len       = (float)summ->total_length /
                         (float)summ->comparison_count;
  float mean_refr_exon = (float)summ->refr_exon_count /
                         (float)summ->comparison_count;
  float mean_pred_exon = (float)summ->pred_exon_count /
                         (float)summ->comparison_count;
  float mean_refr_cds  = (float)summ->refr_cds_length / 3 /
                         (float)summ->comparison_count;
  float mean_pred_cds  = (float)summ->pred_cds_length / 3 /
                         (float)summ->comparison_count;

  if(summ->comparison_count > 0)
  {
    fprintf(outstream, "        <tr><td><a href=\"%s.html\">(+)</a> %s ",
            compkey, label);
  }
  else
  {
    fprintf(outstream, "        <tr><td>(+) %s ", label);
  }

  fprintf(outstream,
          "<span class=\"tooltip\">"
          "<span class=\"small_tooltip\">[?]</span>"
          "<span class=\"tooltip_text\">%s</span></span></td>"
          "<td>%lu (%.1f%%)</tr>\n", desc, numclass, perc_class);

  if(numclass == 0)
    return;

  fprintf(outstream,
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average length</td><td>%.2lf bp</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average # refr exons</td><td>%.2lf</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average # pred exons</td><td>%.2lf</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average refr CDS length</td><td>%.2lf aa</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average pred CDS length</td><td>%.2lf aa</td></tr>\n",
          mean_len,mean_refr_exon,mean_pred_exon,mean_refr_cds,mean_pred_cds);
}

static void compare_report_html_footer(FILE *outstream)
{
  fprintf(outstream,
          "      <p class=\"footer\">\n"
          "        Generated by <a href=\"%s\">AEGeAn %s (%s %s)</a>.<br />\n"
          "        Copyright © %s <a href=\"http://aegean.readthedocs.io/en/"
          "latest/contrib.html\">AEGeAn authors</a>.<br />\n"
          "        See <a href=\"LICENSE\">LICENSE</a> for details."
          "      </p>\n", AGN_VERSION_LINK, AGN_SEMANTIC_VERSION,
          AGN_VERSION_STABILITY, AGN_VERSION_HASH_SLUG, AGN_COPY_YEAR);
}

static void compare_report_html_free(GtNodeVisitor *nv)
{
  AgnCompareReportHTML *rpt;
  agn_assert(nv);

  rpt = compare_report_html_cast(nv);
  gt_str_array_delete(rpt->seqids);
  gt_hashmap_delete(rpt->seqdata);
  gt_hashmap_delete(rpt->seqlocusdata);
  gt_hashmap_delete(rpt->compclassdata);
  gt_str_delete(rpt->summary_title);
}

static void compare_report_html_locus_gene_ids(AgnLocus *locus, FILE *outstream)
{
  GtUword i;
  GtArray *refr_genes = agn_locus_refr_gene_ids(locus);
  GtArray *pred_genes = agn_locus_pred_gene_ids(locus);

  fputs("      <h2>Gene annotations</h2>\n"
        "      <table>\n"
        "        <tr><th>Reference</th><th>Prediction</th></tr>\n",
        outstream);
  for(i = 0; i < gt_array_size(refr_genes) || i < gt_array_size(pred_genes); i++)
  {
    fputs("        <tr>", outstream);
    if(i < gt_array_size(refr_genes))
    {
      const char *gid = *(const char **)gt_array_get(refr_genes, i);
      fprintf(outstream, "<td>%s</td>", gid);
    }
    else
    {
      if(i == 0) fputs("<td>None</td>", outstream);
      else       fputs("<td>&nbsp;</td>", outstream);
    }

    if(i < gt_array_size(pred_genes))
    {
      const char *gid = *(const char **)gt_array_get(pred_genes, i);
      fprintf(outstream, "<td>%s</td>", gid);
    }
    else
    {
      if(i == 0) fputs("<td>None</td>", outstream);
      else       fputs("<td>&nbsp;</td>", outstream);
    }
    fputs("</tr>\n", outstream);
  }
  fputs("      </table>\n\n", outstream);
  gt_array_delete(refr_genes);
  gt_array_delete(pred_genes);
}

static void compare_report_html_locus_handler(AgnCompareReportHTML *rpt,
                                              AgnLocus *locus)
{
  GtArray *pairs2report, *unique;
  GtUword i;

  GtStr *seqid = gt_genome_node_get_seqid(locus);
  GtRange rng = gt_genome_node_get_range(locus);
  compare_report_html_save_seq_locus_data(rpt, locus);
  AgnComparisonData *seqdat = gt_hashmap_get(rpt->seqdata, gt_str_get(seqid));
  agn_locus_data_aggregate(locus, seqdat);

  char filename[1024];
  sprintf(filename, "%s/%s/%lu-%lu.html", rpt->outdir, gt_str_get(seqid),
          rng.start, rng.end);
  FILE *outstream = fopen(filename, "w");
  if(outstream == NULL)
  {
    fprintf(stderr, "error: unable to open output file '%s'\n", filename);
    exit(1);
  }

  compare_report_html_locus_header(locus, outstream);
#ifndef WITHOUT_CAIRO
  if(rpt->pngdata != NULL)
  {
    agn_locus_print_png(locus, rpt->pngdata);
    fprintf(outstream,
            "      <div class='graphic'>\n"
            "        <a href='%s_%lu-%lu.png'><img src='%s_%lu-%lu.png' /></a>\n"
            "      </div>\n\n",
            gt_str_get(seqid), rng.start, rng.end,
            gt_str_get(seqid), rng.start, rng.end);
  }
#endif

  pairs2report = agn_locus_pairs_to_report(locus);
  if(pairs2report == NULL || gt_array_size(pairs2report) == 0)
  {
    fputs("      <p>No comparisons were performed for this locus.</p>\n\n",
          outstream);
    compare_report_html_footer(outstream);
    fclose(outstream);
    return;
  }

  fputs("      <h2 class=\"bottomspace\">Comparison(s)</h2>\n", outstream);
  for(i = 0; i < gt_array_size(pairs2report); i++)
  {
    AgnCliquePair *pair = *(AgnCliquePair **)gt_array_get(pairs2report, i);
    bool collapse = gt_array_size(pairs2report) > 1;
    compare_report_html_print_pair(pair, outstream, i, collapse, rpt->gff3);
  }

  unique = agn_locus_get_unique_refr_cliques(locus);
  if(gt_array_size(unique) > 0)
  {
    fputs("      <h2>Unmatched reference transcripts</h2>\n"
          "      <ul>\n",
          outstream);
    for(i = 0; i < gt_array_size(unique); i++)
    {
      GtUword j;
      AgnTranscriptClique **clique = gt_array_get(unique, i);
      GtArray *ids = agn_transcript_clique_ids(*clique);
      for(j = 0; j < gt_array_size(ids); j++)
      {
        const char *id = *(const char **)gt_array_get(ids, j);
        fprintf(outstream, "        <li>%s</li>\n", id);
      }
      gt_array_delete(ids);
    }
    fputs("      </ul>\n\n", outstream);
  }

  unique = agn_locus_get_unique_pred_cliques(locus);
  if(gt_array_size(unique) > 0)
  {
    fputs("      <h2>Unmatched prediction transcripts</h2>\n"
          "      <ul>\n",
          outstream);
    for(i = 0; i < gt_array_size(unique); i++)
    {
      GtUword j;
      AgnTranscriptClique **clique = gt_array_get(unique, i);
      GtArray *ids = agn_transcript_clique_ids(*clique);
      for(j = 0; j < gt_array_size(ids); j++)
      {
        const char *id = *(const char **)gt_array_get(ids, j);
        fprintf(outstream, "        <li>%s</li>\n", id);
      }
      gt_array_delete(ids);
    }
    fputs("      </ul>\n\n", outstream);
  }
  fputs("\n", outstream);

  compare_report_html_footer(outstream);
  fputs("    </div>\n"
        "  </body>\n"
        "</html>",
        outstream);
  fclose(outstream);
}

static void compare_report_html_locus_header(AgnLocus *locus, FILE *outstream)
{
  GtRange range = gt_genome_node_get_range(locus);
  GtStr *seqid = gt_genome_node_get_seqid(locus);
  GtArray *pairs2report = agn_locus_pairs_to_report(locus);
  GtUword numpairs = gt_array_size(pairs2report);

  fprintf( outstream,
           "<!doctype html>\n"
           "<html lang=\"en\">\n"
           "  <head>\n"
           "    <meta charset=\"utf-8\" />\n"
           "    <title>ParsEval: Locus at %s[%lu, %lu]</title>\n"
           "    <link rel=\"stylesheet\" type=\"text/css\" "
           "href=\"../parseval.css\" />\n",
           gt_str_get(seqid), range.start, range.end);

  if(numpairs > 1)
  {
    GtUword i;
    fputs("    <script type=\"text/javascript\""
          " src=\"../vendor/mootools-core-1.3.2-full-nocompat-yc.js\"></script>\n"
          "    <script type=\"text/javascript\" src=\"../vendor/mootools-more-1.3.2.1.js\"></script>\n"
          "    <script type=\"text/javascript\">\n"
          "window.addEvent('domready', function() {\n"
          "  var status =\n"
          "  {\n"
          "    'true': \"(hide details)\",\n"
          "    'false': \"(show details)\",\n"
          "  }\n",
          outstream);
    for(i = 0; i < numpairs; i++)
    {
      fprintf(outstream,
              "  var compareWrapper%lu = new Fx.Slide('compare_wrapper_%lu');\n"
              "  compareWrapper%lu.hide();\n"
              "  $('toggle_compare_%lu').addEvent('click', function(event){\n"
              "    event.stop();\n"
              "    compareWrapper%lu.toggle();\n"
              "  });\n"
              "  compareWrapper%lu.addEvent('complete', function() {\n"
              "    $('toggle_compare_%lu').set('text', status[compareWrapper%lu.open]);\n"
              "  });\n",
              i, i, i, i, i, i, i, i);
    }
    fputs("});\n"
          "    </script>\n",
          outstream);
  }

  fprintf(outstream,
          "  </head>\n"
          "  <body>\n"
          "    <div id=\"content\">\n"
          "      <h1>Locus at %s[%lu, %lu]</h1>\n"
          "      <p><a href=\"index.html\">⇐ Back to %s loci</a></p>\n\n",
          gt_str_get(seqid), range.start, range.end, gt_str_get(seqid) );

  compare_report_html_locus_gene_ids(locus, outstream);
}

static void compare_report_html_pair_nucleotide(FILE *outstream,
                                                AgnCliquePair *pair)
{
  AgnComparison *pairstats = agn_clique_pair_get_stats(pair);
  double identity = (double)pairstats->overall_matches /
                    (double)pairstats->overall_length;
  if(pairstats->overall_matches == pairstats->overall_length)
    fputs("        <h3>Gene structures match perfectly!</h3>\n", outstream);
  else
  {
    fprintf(outstream,
            "        <h3>Nucleotide-level comparison</h3>\n"
            "        <table class=\"table_wide table_extra_indent\">\n"
            "          <tr><td>&nbsp;</td><th>CDS</th><th>UTRs</th><th>Overall</th></tr>\n"
            "          <tr><th class=\"left-align\">matching coefficient</th><td>%-10s</td><td>%-10s</td><td>%.3f</td></tr>\n"
            "          <tr><th class=\"left-align\">correlation coefficient</th><td>%-10s</td><td>%-10s</td><td>--</td></tr>\n"
            "          <tr><th class=\"left-align\">sensitivity</th><td>%-10s</td><td>%-10s</td><td>--</td></tr>\n"
            "          <tr><th class=\"left-align\">specificity</th><td>%-10s</td><td>%-10s</td><td>--</td></tr>\n"
            "          <tr><th class=\"left-align\">F1 Score</th><td>%-10s</td><td>%-10s</td><td>--</td></tr>\n"
            "          <tr><th class=\"left-align\">Annotation edit distance</th><td>%-10s</td><td>%-10s</td><td>--</td></tr>\n"
            "        </table>\n",
            pairstats->cds_nuc_stats.mcs, pairstats->utr_nuc_stats.mcs,
            identity, pairstats->cds_nuc_stats.ccs,
            pairstats->utr_nuc_stats.ccs, pairstats->cds_nuc_stats.sns,
            pairstats->utr_nuc_stats.sns, pairstats->cds_nuc_stats.sps,
            pairstats->utr_nuc_stats.sps, pairstats->cds_nuc_stats.f1s,
            pairstats->utr_nuc_stats.f1s, pairstats->cds_nuc_stats.eds,
            pairstats->utr_nuc_stats.eds);
  }
}

static void compare_report_html_pair_structure(FILE *outstream,
                                               AgnCompStatsBinary *stats,
                                               const char *label,
                                               const char *units)
{
  fprintf(outstream,
          "        <h3>%s structure comparison</h3>\n"
          "        <table class=\"table_normal table_extra_indent\">\n",
          label);
  if(stats->missing == 0 && stats->wrong == 0)
  {
    fprintf(outstream,
            "          <tr><td>reference %s</td><td>%lu</td></tr>\n"
            "          <tr><td>prediction %s</td><td>%lu</td></tr>\n"
            "          <tr><th class=\"left-align\" colspan=\"2\">%s structures"
            " match perfectly!</th></tr>\n",
            units, stats->correct, units, stats->correct, units);
  }
  else
  {
    fprintf(outstream,
            "          <tr><td>reference %s</td><td>%lu</td></tr>\n"
            "          <tr class=\"cell_small\"><td class=\"cell_indent\">match"
            " prediction</td><td>%lu</td></tr>\n"
            "          <tr class=\"cell_small\"><td class=\"cell_indent\">don't"
            " match prediction</td><td>%lu</td></tr>\n"
            "          <tr><td>prediction %s</td><td>%lu</td></tr>\n"
            "          <tr class=\"cell_small\"><td class=\"cell_indent\">match"
            " reference</td><td>%lu</td></tr>\n"
            "          <tr class=\"cell_small\"><td class=\"cell_indent\">don't"
            " match reference</td><td>%lu</td></tr>\n",
             units, stats->correct + stats->missing,
             stats->correct, stats->missing,
             units, stats->correct + stats->wrong,
             stats->correct, stats->wrong);
    fprintf(outstream,
            "          <tr><td>Sensitivity</td><td>%-10s</td></tr>\n"
            "          <tr><td>Specificity</td><td>%-10s</td></tr>\n"
            "          <tr><td>F1 score</td><td>%-10s</td></tr>\n"
            "          <tr><td>Annotation edit distance</td><td>%-10s</td></tr>\n",
            stats->sns, stats->sps, stats->f1s, stats->eds);
  }
  fputs("        </table>\n\n", outstream);
}



static void compare_report_html_print_compclassfiles(AgnCompareReportHTML *rpt)
{
  GtArray *compclassdata;
  GtUword i;

  compclassdata = gt_hashmap_get(rpt->compclassdata, "perfect");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/perfectmatches.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "perfect matches");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }

  compclassdata = gt_hashmap_get(rpt->compclassdata, "mislabeled");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/mislabeled.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "perfect matches with "
                                         "mislabeled UTRs");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }

  compclassdata = gt_hashmap_get(rpt->compclassdata, "cds");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/cdsmatches.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "CDS matches");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }

  compclassdata = gt_hashmap_get(rpt->compclassdata, "exon");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/exonmatches.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "exon matches");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }

  compclassdata = gt_hashmap_get(rpt->compclassdata, "utr");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/utrmatches.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "UTR matches");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }

  compclassdata = gt_hashmap_get(rpt->compclassdata, "nonmatch");
  if(gt_array_size(compclassdata) > 0)
  {
    char filename[AGN_MAX_FILENAME_SIZE];
    sprintf(filename, "%s/nonmatches.html", rpt->outdir);
    FILE *outstream = fopen(filename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", filename);
      exit(1);
    }
    compare_report_html_compclass_header(outstream, "non-matches");

    for(i = 0; i < gt_array_size(compclassdata); i++)
    {
      SeqfileLocusData *data = gt_array_get(compclassdata, i);
      compare_report_html_print_locus_to_seqfile(data, true, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }
}

static void
compare_report_html_print_locus_to_seqfile(SeqfileLocusData *data,
                                           bool printseqid, FILE *outstream)
{
  char sstart[64], send[64], slength[64];
  agn_sprintf_comma(data->lrange.start, sstart);
  agn_sprintf_comma(data->lrange.end, send);
  agn_sprintf_comma(gt_range_length(&data->lrange), slength);
  if(printseqid)
  {
    fprintf(outstream,
            "        <tr>\n"
            "          <td><a href=\"%s/%lu-%lu.html\">(+)</a></td>\n"
            "          <td>%s</td>\n",
            data->seqid, data->lrange.start, data->lrange.end, data->seqid);
  }
  else
  {
    fprintf(outstream,
            "        <tr>\n"
            "          <td><a href=\"%lu-%lu.html\">(+)</a></td>\n",
            data->lrange.start, data->lrange.end);
  }

  fprintf(outstream,
          "          <td>%s</td>\n"
          "          <td>%s</td>\n"
          "          <td>%s</td>\n"
          "          <td>%lu / %lu</td>\n"
          "          <td>\n",
          sstart, send, slength, data->refrtrans, data->predtrans);
  if(data->numperfect > 0)
  {
    fprintf(outstream,
            "            <a class=\"pointer left20\" title=\"Perfect "
            "matches at this locus\">[P]</a> %u\n", data->numperfect);
  }
  if(data->nummislabeled > 0)
  {
    fprintf(outstream,
            "            <a class=\"pointer left20\" title=\"Perfect "
            "matches at this locus with mislabeled UTRs\">[M]</a> %u\n",
            data->nummislabeled);
  }
  if(data->numcdsmatch > 0)
  {
    fprintf(outstream,
            "            <a class=\"pointer left20\" title=\"CDS "
            "matches at this locus\">[C]</a> %u\n", data->numcdsmatch);
  }
  if(data->numexonmatch > 0)
  {
    fprintf(outstream,
            "            <a class=\"pointer left20\" title=\"Exon "
            "structure matches at this locus\">[E]</a> %u\n",
            data->numexonmatch);
  }
  if(data->numutrmatch > 0)
  {
    fprintf(outstream,
            "            <a class=\"pointer left20\" title=\"UTR "
            "matches at this locus\">[U]</a> %u\n", data->numutrmatch);
  }
  if(data->numnonmatch > 0)
  {
     fprintf(outstream, "            <a class=\"pointer left20\" "
             "title=\"Non-matches at this locus\">[N]</a> %u\n",
             data->numnonmatch);
  }
  fprintf(outstream, "          </td>\n"
          "        </tr>\n");
}

static void compare_report_html_print_pair(AgnCliquePair *pair, FILE *outstream,
                                           GtUword k, bool collapse, bool gff3)
{
  AgnTranscriptClique *refrclique = agn_clique_pair_get_refr_clique(pair);
  AgnTranscriptClique *predclique = agn_clique_pair_get_pred_clique(pair);

  if(collapse)
  {
    fprintf(outstream, "      <h3 class=\"compare-header\">Comparison"
            "<a id=\"toggle_compare_%lu\" href=\"#\">(show details)</a></h3>\n",
            k);
  }
  fprintf(outstream, "      <div id=\"compare_wrapper_%lu\""
                     "class=\"compare-wrapper\">\n", k);

  if(gff3)
  {
    fputs("        <h3>Reference GFF3</h3>\n"
          "        <pre class=\"gff3 refr\">\n", outstream);
    agn_transcript_clique_to_gff3(refrclique, outstream, NULL);
    fputs("</pre>\n", outstream);
    fputs("        <h3>Prediction GFF3</h3>\n"
          "        <pre class=\"gff3 pred\">\n", outstream);
    agn_transcript_clique_to_gff3(predclique, outstream, NULL);
    fputs("</pre>\n", outstream);
  }

  AgnComparison *pairstats = agn_clique_pair_get_stats(pair);
  compare_report_html_pair_structure(outstream, &pairstats->cds_struc_stats,
                                     "CDS", "CDS segments");
  compare_report_html_pair_structure(outstream, &pairstats->exon_struc_stats,
                                     "Exon", "exons");
  compare_report_html_pair_structure(outstream, &pairstats->utr_struc_stats,
                                     "UTR", "UTR segments");
  compare_report_html_pair_nucleotide(outstream, pair);

  fputs("      </div>\n\n", outstream);
}

static void compare_report_html_print_seqfiles(AgnCompareReportHTML *rpt)
{
  GtUword i, j;
  for(i = 0; i < gt_str_array_size(rpt->seqids); i++)
  {
    const char *seqid = gt_str_array_get(rpt->seqids, i);
    char seqfilename[AGN_MAX_FILENAME_SIZE];
    sprintf(seqfilename, "%s/%s/index.html", rpt->outdir, seqid);
    FILE *outstream = fopen(seqfilename, "w");
    if(!outstream)
    {
      fprintf(stderr, "error: unable to open %s\n", seqfilename);
      exit(1);
    }
    compare_report_html_seqfile_header(outstream, seqid);

    GtArray *sld = gt_hashmap_get(rpt->seqlocusdata, seqid);
    agn_assert(sld);
    for(j = 0; j < gt_array_size(sld); j++)
    {
      SeqfileLocusData *data = gt_array_get(sld, j);
      compare_report_html_print_locus_to_seqfile(data, false, outstream);
    }
    compare_report_html_seqfile_footer(outstream);
    fclose(outstream);
  }
}

static void
compare_report_html_save_seq_locus_data(AgnCompareReportHTML *rpt,
                                        AgnLocus *locus)
{
  SeqfileLocusData data;
  GtArray *pairs2report, *seqlocusdata;
  GtStr *seqid;
  GtUword i;

  seqid = gt_genome_node_get_seqid(locus);
  snprintf(data.seqid, 64, "%s", gt_str_get(seqid));
  data.lrange = gt_genome_node_get_range(locus);
  data.refrtrans = agn_locus_num_refr_mrnas(locus);
  data.predtrans = agn_locus_num_pred_mrnas(locus);

  data.numperfect = 0;
  data.nummislabeled = 0;
  data.numcdsmatch = 0;
  data.numexonmatch = 0;
  data.numutrmatch = 0;
  data.numnonmatch = 0;

  pairs2report = agn_locus_pairs_to_report(locus);
  for(i = 0; i < gt_array_size(pairs2report); i++)
  {
    AgnCliquePair *pair = *(AgnCliquePair **)gt_array_get(pairs2report, i);
    AgnCompClassification cls = agn_clique_pair_classify(pair);
    if     (cls == AGN_COMP_CLASS_PERFECT_MATCH) data.numperfect++;
    else if(cls == AGN_COMP_CLASS_MISLABELED)    data.nummislabeled++;
    else if(cls == AGN_COMP_CLASS_CDS_MATCH)     data.numcdsmatch++;
    else if(cls == AGN_COMP_CLASS_EXON_MATCH)    data.numexonmatch++;
    else if(cls == AGN_COMP_CLASS_UTR_MATCH)     data.numutrmatch++;
    else if(cls == AGN_COMP_CLASS_NON_MATCH)     data.numnonmatch++;
    else if(cls == AGN_COMP_CLASS_UNCLASSIFIED)  agn_assert(false);
    else agn_assert(false);
  }

  seqlocusdata = gt_hashmap_get(rpt->seqlocusdata, gt_str_get(seqid));
  gt_array_add(seqlocusdata, data);

  GtArray *ccdata;
  if(data.numperfect > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "perfect");
    gt_array_add(ccdata, data);
  }
  if(data.nummislabeled > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "mislabeled");
    gt_array_add(ccdata, data);
  }
  if(data.numcdsmatch > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "cds");
    gt_array_add(ccdata, data);
  }
  if(data.numexonmatch > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "exon");
    gt_array_add(ccdata, data);
  }
  if(data.numutrmatch > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "utr");
    gt_array_add(ccdata, data);
  }
  if(data.numnonmatch > 0)
  {
    ccdata = gt_hashmap_get(rpt->compclassdata, "nonmatch");
    gt_array_add(ccdata, data);
  }
}

static void compare_report_html_seqfile_header(FILE *outstream,
                                               const char *seqid)
{
  fprintf(outstream,
          "<!doctype html>\n"
          "<html lang=\"en\">\n"
          "  <head>\n"
          "    <meta charset=\"utf-8\" />\n"
          "    <title>ParsEval: Loci for %s</title>\n"
          "    <link rel=\"stylesheet\" type=\"text/css\" href=\"../parseval.css\" />\n"
          "    <script type=\"text/javascript\" language=\"javascript\" src=\"../vendor/jquery.js\"></script>\n"
          "    <script type=\"text/javascript\" language=\"javascript\" src=\"../vendor/jquery.dataTables.js\"></script>\n"
          "    <script type=\"text/javascript\">\n"
          "      $(document).ready(function() {\n"
          "        $('#locus_table').dataTable( {\n"
          "          \"sScrollY\": \"400px\",\n"
          "          \"bPaginate\": false,\n"
          "          \"bScrollCollapse\": true,\n"
          "          \"bSort\": false,\n"
          "          \"bFilter\": false,\n"
          "          \"bInfo\": false\n"
          "        });\n"
          "      } );\n"
          "    </script>\n"
          "  </head>\n"
          "  <body>\n"
          "    <div id=\"content\">\n"
          "      <h1>Loci for %s</h1>\n"
          "      <p><a href=\"../index.html\">⇐ Back to summary</a></p>\n\n"
          "      <p class=\"indent\">\n"
          "        Below is a list of all loci identified for sequence <strong>%s</strong>.\n"
          "        Click on the <a>(+)</a> symbol for a report of the complete comparative analysis corresponding to each locus.\n"
          "      </p>\n\n"
          "      <table class=\"loci\" id=\"locus_table\">\n"
          "        <thead>\n"
          "          <tr>\n"
          "            <th>&nbsp;</th>\n"
          "            <th>Start</th>\n"
          "            <th>End</th>\n"
          "            <th>Length</th>\n"
          "            <th>#Trans</th>\n"
          "            <th>Comparisons</th>\n"
          "          </tr>\n"
          "        </thead>\n"
          "        <tbody>\n",
          seqid, seqid, seqid);
}

static void compare_report_html_seqfile_footer(FILE *outstream)
{
  fputs("        </tbody>\n", outstream);
  fputs("      </table>\n\n", outstream);
  compare_report_html_footer(outstream);
  fputs("    </div>\n", outstream);
  fputs("  </body>\n", outstream);
  fputs("</html>\n", outstream);
}

static void compare_report_html_summary_annot(AgnCompInfo *info,
                                              FILE *outstream)
{
  GtUword numnotshared = info->unique_refr_loci +
                         info->unique_pred_loci;
  fprintf(outstream,
          "      <h2>Gene loci <span class=\"tooltip\">[?]<span class=\"tooltip_text\">If a gene "
          "annotation overlaps with another gene annotation, those annotations are associated "
          "with the same gene locus. See <a target=\"_blank\" "
          "href=\"http://aegean.readthedocs.io/en/latest/loci.html\">"
          "this page</a> for a formal definition of a locus annotation.</span></span></h2>\n"
          "      <table class=\"table_normal\">\n"
          "        <tr><td>shared</td><td>%lu</td></tr>\n"
          "        <tr><td>unique to reference</td><td>%lu</td></tr>\n"
          "        <tr><td>unique to prediction</td><td>%lu</td></tr>\n"
          "        <tr><th class=\"right-align\">Total</th><td>%lu</td></tr>\n"
          "      </table>\n\n",
          info->num_loci - numnotshared, info->unique_refr_loci,
          info->unique_pred_loci, info->num_loci);

  fprintf(outstream,
          "      <h2>Reference annotations</h2>\n"
          "      <table class=\"table_normal\">\n"
          "        <tr><td>genes</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per locus</td><td>%.3f</td></tr>\n"
          "        <tr><td>transcripts</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per locus</td><td>%.3f</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per gene</td><td>%.3f</td></tr>\n"
          "      </table>\n\n",
          info->refr_genes, (float)info->refr_genes / (float)info->num_loci,
          info->refr_transcripts,
          (float)info->refr_transcripts / (float)info->num_loci,
          (float)info->refr_transcripts / (float)info->refr_genes);

  fprintf(outstream,
          "      <h2>Prediction annotations</h2>\n"
          "      <table class=\"table_normal\">\n"
          "        <tr><td>genes</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per locus</td><td>%.3f</td></tr>\n"
          "        <tr><td>transcripts</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per locus</td><td>%.3f</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">average per gene</td><td>%.3f</td></tr>\n"
          "      </table>\n\n",
          info->pred_genes, (float)info->pred_genes / (float)info->num_loci,
          info->pred_transcripts,
          (float)info->pred_transcripts / (float)info->num_loci,
          (float)info->pred_transcripts / (float)info->pred_genes);
}

static void compare_report_html_summary_body(AgnCompareReportHTML *rpt,
                                             FILE *outstream)
{
  AgnComparisonData *data = &rpt->data;

  fprintf(outstream,"      <h2>Comparisons</h2>\n"
                    "      <table class=\"comparisons\">\n"
                    "        <tr><th>Total comparisons</th><th>%lu</th></tr>\n",
          data->info.num_comparisons);
  compare_report_html_comp_class_summary(
      &data->summary.perfect_matches, data->info.num_comparisons,
      "perfect matches", "perfectmatches", "Prediction transcripts (exons, "
      "coding sequences, and UTRs) line up perfectly with reference "
      "transcripts.", outstream);
  compare_report_html_comp_class_summary(
      &data->summary.perfect_mislabeled, data->info.num_comparisons,
      "perfect matches with mislabeled UTRs", "mislabeled", "5'/3' orientation "
      "of UTRs is reversed between reference and prediction, but a perfect "
      "match in all other aspects.", outstream);
  compare_report_html_comp_class_summary(
      &data->summary.cds_matches, data->info.num_comparisons,
      "CDS structure matches", "cdsmatches", "Not a perfect match, but "
      "prediction coding sequence(s) line up perfectly with reference coding "
      "sequence(s).", outstream);
  compare_report_html_comp_class_summary(
      &data->summary.exon_matches, data->info.num_comparisons,
      "exon structure matches", "exonmatches", "Not a perfect match or CDS "
      "match, but prediction exon structure is identical to reference exon "
      "structure.", outstream);
  compare_report_html_comp_class_summary(
      &data->summary.utr_matches, data->info.num_comparisons,
      "UTR structure matches", "utrmatches", "Not a perfect match, CDS match, "
      "or exon structure match, but prediction UTRs line up perfectly with "
      "reference UTRs.", outstream);
  compare_report_html_comp_class_summary(
      &data->summary.non_matches, data->info.num_comparisons, "non-matches",
      "nonmatches", "Differences in CDS, exon, and UTR structure.", outstream);
  fputs("      </table>\n\n", outstream);

  compare_report_html_summary_struc(outstream, &data->stats.cds_struc_stats,
                                    "CDS", "CDS segments");
  compare_report_html_summary_struc(outstream, &data->stats.exon_struc_stats,
                                    "Exon", "exons");
  compare_report_html_summary_struc(outstream, &data->stats.utr_struc_stats,
                                    "UTR", "UTR segments");

  double identity = (double)data->stats.overall_matches /
                    (double)data->stats.overall_length;
  fprintf(outstream,
          "      <h3>Nucleotide-level comparison</h3>\n"
          "      <table class=\"table_wide table_extra_indent\">\n"
          "        <tr><th>&nbsp;</th><th>CDS</th><th>UTRs</th><th>Overall</th></tr>\n"
          "        <tr><th class=\"left-align\">matching coefficient</th><td>%s</td><td>%s</td><td>%.3lf</td></tr>\n"
          "        <tr><th class=\"left-align\">correlation coefficient</th><td>%s</td><td>%s</td><td>--</td></tr>\n"
          "        <tr><th class=\"left-align\">sensitivity</th><td>%s</td><td>%s</td><td>--</td></tr>\n"
          "        <tr><th class=\"left-align\">specificity</th><td>%s</td><td>%s</td><td>--</td></tr>\n"
          "        <tr><th class=\"left-align\">F1 score</th><td>%s</td><td>%s</td><td>--</td></tr>\n"
          "        <tr><th class=\"left-align\">annotation edit distance</th><td>%s</td><td>%s</td><td>--</td></tr>\n"
          "      </table>\n\n",
          data->stats.cds_nuc_stats.mcs, data->stats.utr_nuc_stats.mcs, identity,
          data->stats.cds_nuc_stats.ccs, data->stats.utr_nuc_stats.ccs,
          data->stats.cds_nuc_stats.sns, data->stats.utr_nuc_stats.sns,
          data->stats.cds_nuc_stats.sps, data->stats.utr_nuc_stats.sps,
          data->stats.cds_nuc_stats.f1s, data->stats.utr_nuc_stats.f1s,
          data->stats.cds_nuc_stats.eds, data->stats.utr_nuc_stats.eds);
}

static void compare_report_html_summary_headmatter(AgnCompareReportHTML *rpt,
                                                   FILE *outstream)
{
  GtUword i;
  AgnComparisonData *data = &rpt->data;

  fprintf(outstream,
          "<!doctype html>\n"
          "<html lang=\"en\">\n"
          "  <head>\n"
          "    <meta charset=\"utf-8\" />\n"
          "    <title>%s</title>\n"
          "    <link rel=\"stylesheet\" type=\"text/css\" "
          "href=\"parseval.css\" />\n"
          "    <script type=\"text/javascript\" language=\"javascript\" "
          "src=\"vendor/jquery.js\"></script>\n"
          "    <script type=\"text/javascript\" language=\"javascript\" "
          "src=\"vendor/jquery.dataTables.js\"></script>\n"
          "    <script type=\"text/javascript\">\n"
          "      $(document).ready(function() {\n"
          "        $('#seqlist').dataTable( {\n"
          "          \"sScrollY\": \"400px\",\n"
          "          \"bPaginate\": false,\n"
          "          \"bScrollCollapse\": true,\n"
          "          \"bSort\": false,\n"
          "          \"bFilter\": false,\n"
          "          \"bInfo\": false\n"
          "        });\n"
          "      } );\n"
          "    </script>\n"
          "  </head>\n"
          "  <body>\n"
          "    <div id=\"content\">\n"
          "      <h1>%s</h1>\n",
          gt_str_get(rpt->summary_title),
          gt_str_get(rpt->summary_title));

  // Print runtime overview
  if(rpt->ofunc != NULL)
    rpt->ofunc(outstream, rpt->ofuncdata);

  // Print a table with information about each sequence, including a link to
  // each sequence's detail page.
  fputs("      <h2>Sequences compared</h2>\n"
        "      <p class=\"indent\">Click on a sequence ID below to see "
        "comparison results for individual loci.</p>\n", outstream);
  fputs("      <table id=\"seqlist\" class=\"indent\">\n"
        "        <thead>\n"
        "          <tr>\n"
        "            <th>Sequence</th>\n"
        "            <th>Refr genes</th>\n"
        "            <th>Pred genes</th>\n"
        "            <th>Loci</th>\n"
        "          </tr>\n"
        "        </thead>\n"
        "        <tbody>\n", outstream);
  for(i = 0; i < gt_str_array_size(rpt->seqids); i++)
  {
    const char *seqid = gt_str_array_get(rpt->seqids, i);
    AgnComparisonData *seqdat = gt_hashmap_get(rpt->seqdata, seqid);
    agn_assert(data != NULL);
    fprintf(outstream,
            "        <tr><td><a href=\"%s/index.html\">%s</a></td>"
            "<td>%lu</td><td>%lu</td><td>%lu</td></tr>\n",
            seqid, seqid, seqdat->info.refr_genes, seqdat->info.pred_genes,
            seqdat->info.num_loci);
  }
  fputs("        </tbody>\n"
        "      </table>\n\n", outstream);

  compare_report_html_summary_annot(&data->info, outstream);
}

static void compare_report_html_summary_struc(FILE *outstream,
                                              AgnCompStatsBinary *stats,
                                              const char *label,
                                              const char *units)
{
  fprintf(outstream, "      <h3>%s structure comparison</h3>\n", label);

  GtUword refrcnt = stats->correct + stats->missing;
  GtUword predcnt = stats->correct + stats->wrong;
  char rmatchp[32], rnomatchp[32], pmatchr[32], pnomatchr[32];
  if(refrcnt > 0)
  {
    sprintf(rmatchp,   "%.1f%%", (float)stats->correct / (float)refrcnt * 100);
    sprintf(rnomatchp, "%.1f%%", (float)stats->missing / (float)refrcnt * 100);
  }
  else
  {
    sprintf(rmatchp,   "--");
    sprintf(rnomatchp, "--");
  }
  if(predcnt > 0)
  {
    sprintf(pmatchr,   "%.1f%%", (float)stats->correct / (float)predcnt * 100);
    sprintf(pnomatchr, "%.1f%%", (float)stats->wrong   / (float)predcnt * 100);
  }
  else
  {
    sprintf(pmatchr,   "--");
    sprintf(pnomatchr, "--");
  }

  fprintf(outstream,
          "      <table class=\"table_normal table_extra_indent\">\n"
          "        <tr><td>reference %s</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">match prediction</td><td>%lu (%s)</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">don't match prediction</td><td>%lu (%s)</td></tr>\n"
          "        <tr><td>prediction %s</td><td>%lu</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">match prediction</td><td>%lu (%s)</td></tr>\n"
          "        <tr class=\"cell_small\"><td class=\"cell_indent\">don't match prediction</td><td>%lu (%s)</td></tr>\n"
          "        <tr><td>Sensitivity</td><td>%s</td></tr>\n"
          "        <tr><td>Specificity</td><td>%s</td></tr>\n"
          "        <tr><td>F1 score</td><td>%s</td></tr>\n"
          "        <tr><td>Annotation edit distance</td><td>%s</td></tr>\n"
          "      </table>\n\n",
          units, stats->correct + stats->missing, stats->correct, rmatchp,
          stats->missing, rnomatchp, units, stats->correct + stats->wrong,
          stats->correct, pmatchr, stats->wrong, pnomatchr,
          stats->sns, stats->sps, stats->f1s, stats->eds);
}

static int compare_report_html_visit_feature_node(GtNodeVisitor *nv,
                                                  GtFeatureNode *fn,
                                                  GtError *error)
{
  AgnCompareReportHTML *rpt;
  AgnLocus *locus;

  gt_error_check(error);
  agn_assert(nv && fn && gt_feature_node_has_type(fn, "locus"));

  rpt = compare_report_html_cast(nv);
  rpt->locuscount += 1;
  locus = (AgnLocus *)fn;
  agn_locus_comparative_analysis(locus, rpt->logger);
  agn_locus_data_aggregate(locus, &rpt->data);
  compare_report_html_locus_handler(rpt, locus);

  return 0;
}

static int compare_report_html_visit_region_node(GtNodeVisitor *nv,
                                                 GtRegionNode *rn,
                                                 GtError *error)
{
  AgnCompareReportHTML *rpt;
  AgnComparisonData *data;
  GtStr *seqidstr;
  const char *seqid;

  gt_error_check(error);
  agn_assert(nv && rn);

  rpt = compare_report_html_cast(nv);
  seqidstr = gt_genome_node_get_seqid((GtGenomeNode *)rn);
  seqid = gt_cstr_dup(gt_str_get(seqidstr));
  gt_str_array_add(rpt->seqids, seqidstr);
  data = gt_malloc( sizeof(AgnComparisonData) );
  agn_comparison_data_init(data);
  gt_hashmap_add(rpt->seqdata, (char *)seqid, data);

  GtArray *sld = gt_array_new( sizeof(SeqfileLocusData) );
  agn_assert(sld);
  gt_hashmap_add(rpt->seqlocusdata, gt_cstr_dup(seqid), sld);

  char seqdircmd[AGN_MAX_FILENAME_SIZE];
  sprintf(seqdircmd, "mkdir %s/%s", rpt->outdir, seqid);
  if(system(seqdircmd))
  {
    fprintf(stderr, "error: could not create directory %s/%s\n", rpt->outdir,
            seqid);
    exit(1);
  }

  return 0;
}
