//#include "core/queue_api.h"
//#include "extended/array_in_stream_api.h"
//#include "extended/array_out_stream_api.h"
#include "extended/feature_node_iterator_api.h"
#include "AgnInferParentStream.h"
#include "AgnUtils.h"

//------------------------------------------------------------------------------
// Data structure definition
//------------------------------------------------------------------------------

struct AgnInferParentStream
{
  const GtNodeStream parent_instance;
  GtNodeStream *in_stream;
  GtHashmap *type_parents;
};


//------------------------------------------------------------------------------
// Prototypes for private functions
//------------------------------------------------------------------------------

#define infer_parent_stream_cast(GS)\
        gt_node_stream_cast(infer_parent_stream_class(), GS)

/**
 * @function Implements the GtNodeStream interface for this class.
 */
static const GtNodeStreamClass* infer_parent_stream_class(void);

/**
 * @function Class destructor.
 */
static void infer_parent_stream_free(GtNodeStream *ns);

/**
 * @function Implementation of the ``next`` semantic for the more complicated
 * case of a pseudo node.
 */
static void
infer_parent_stream_handle_pseudo(AgnInferParentStream *ips, GtFeatureNode**fn);

/**
 * @function Pulls nodes from the input stream, adds parents as necessary, and
 * then delivers them to the output stream.
 */
static int infer_parent_stream_next(GtNodeStream *ns, GtGenomeNode **gn,
                                    GtError *error);

/**
 * @function Generate data for unit testing.
 */
static void infer_parent_stream_test_data(GtQueue *queue);


//------------------------------------------------------------------------------
// Method implementations
//------------------------------------------------------------------------------

GtNodeStream* agn_infer_parent_stream_new(GtNodeStream *in_stream,
                                          GtHashmap *type_parents)
{
  GtNodeStream *ns;
  AgnInferParentStream *stream;
  gt_assert(in_stream && type_parents);
  ns = gt_node_stream_create(infer_parent_stream_class(), false);
  stream = infer_parent_stream_cast(ns);
  stream->in_stream = gt_node_stream_ref(in_stream);
  stream->type_parents = gt_hashmap_ref(type_parents);
  return ns;
}

static const GtNodeStreamClass *infer_parent_stream_class(void)
{
  static const GtNodeStreamClass *nsc = NULL;
  if(!nsc)
  {
    nsc = gt_node_stream_class_new(sizeof (AgnInferParentStream),
                                   infer_parent_stream_free,
                                   infer_parent_stream_next);
  }
  return nsc;
}

static void infer_parent_stream_free(GtNodeStream *ns)
{
  AgnInferParentStream *stream = infer_parent_stream_cast(ns);
  gt_node_stream_delete(stream->in_stream);
  gt_hashmap_delete(stream->type_parents);
}

static void
infer_parent_stream_handle_pseudo(AgnInferParentStream *ips, GtFeatureNode **fn)
{
  GtGenomeNode *newpseudo = gt_feature_node_new_pseudo_template(*fn);
  GtFeatureNode *newpseudofn = gt_feature_node_cast(newpseudo);

  GtQueue *newparents = gt_queue_new();
  GtHashmap *obsrv_nodes = gt_hashmap_new(GT_HASH_DIRECT, NULL, NULL);
  GtFeatureNodeIterator *iter = gt_feature_node_iterator_new_direct(*fn);
  GtFeatureNode *child;
  for(child  = gt_feature_node_iterator_next(iter);
      child != NULL;
      child  = gt_feature_node_iterator_next(iter))
  {
    const char *fntype = gt_feature_node_get_type(child);
    const char *parenttype = gt_hashmap_get(ips->type_parents, fntype);

    // If no parent inferense is expected for this feature type, just add it to
    // the new pseudo node.
    if(!parenttype)
    {
      gt_genome_node_ref((GtGenomeNode *)child);
      gt_feature_node_add_child(newpseudofn, child);
      continue;
    }

    // If this is part of a multifeature whose new parent has already been
    // inferred, just add this part of the feature to the new parent.
    GtFeatureNode *testfn = child;
    if(gt_feature_node_is_multi(child))
    {
      testfn = gt_feature_node_get_multi_representative(child);
      GtFeatureNode *parent = gt_hashmap_get(obsrv_nodes, testfn);
      if(parent)
      {
        gt_genome_node_ref((GtGenomeNode *)child);
        gt_feature_node_add_child(parent, child);
        continue;
      }
    }

    GtRange childrange = (gt_feature_node_is_multi(child))
                             ? agn_multi_child_range(*fn, testfn)
                             : gt_genome_node_get_range((GtGenomeNode *)child);
    GtStr *seqid = gt_genome_node_get_seqid(*(GtGenomeNode**)fn);
    GtStrand s = gt_feature_node_get_strand(*fn);
    GtGenomeNode *newparent = gt_feature_node_new(seqid, parenttype,
                                                  childrange.start,
                                                  childrange.end, s);
    gt_genome_node_ref((GtGenomeNode *)child);
    gt_feature_node_add_child((GtFeatureNode *)newparent, child);
    gt_hashmap_add(obsrv_nodes, testfn, newparent);
    gt_queue_add(newparents, newparent);
  }
  gt_feature_node_iterator_delete(iter);
  gt_hashmap_delete(obsrv_nodes);
  gt_genome_node_delete(*(GtGenomeNode **)fn);

  GtFeatureNode *newparent;
  while(gt_queue_size(newparents) > 0)
  {
    newparent = gt_queue_get(newparents);
    gt_feature_node_add_child(newpseudofn, newparent);
  }
  gt_queue_delete(newparents);

  if(gt_feature_node_number_of_children(newpseudofn) == 1)
  {
    gt_genome_node_ref((GtGenomeNode *)newparent);
    gt_genome_node_delete(newpseudo);
    newpseudofn = newparent;
  }
  *fn = newpseudofn;
}

static int infer_parent_stream_next(GtNodeStream *ns, GtGenomeNode **gn,
                                    GtError *error)
{
  gt_error_check(error);
  AgnInferParentStream *stream = infer_parent_stream_cast(ns);

  int had_err = gt_node_stream_next(stream->in_stream, gn, error);
  if(had_err)
    return had_err;
  if(!*gn)
    return 0;

  GtFeatureNode *fn = gt_feature_node_try_cast(*gn);
  if(!fn)
    return 0;

  if(gt_feature_node_is_pseudo(fn))
  {
    infer_parent_stream_handle_pseudo(stream, &fn);
    *gn = (GtGenomeNode *)fn;
    return 0;
  }

  const char *fntype = gt_feature_node_get_type(fn);
  const char *parenttype = gt_hashmap_get(stream->type_parents, fntype);
  if(parenttype)
  {
    GtRange fnrange = gt_genome_node_get_range(*gn);
    GtStrand fnstrand = gt_feature_node_get_strand(fn);
    GtStr *seqid = gt_genome_node_get_seqid(*gn);
    GtGenomeNode *parent;
    parent = gt_feature_node_new(seqid, parenttype, fnrange.start,
                                 fnrange.end, fnstrand);
    GtFeatureNode *parentfn = gt_feature_node_cast(parent);
    gt_feature_node_add_child(parentfn, fn);
    *gn = parent;
    return 0;
  }

  return 0;
}

bool agn_infer_parent_stream_unit_test(AgnUnitTest *test)
{
  GtQueue *queue = gt_queue_new();
  infer_parent_stream_test_data(queue);

  agn_unit_test_result(test, "bogus", false);

  while(gt_queue_size(queue) > 0)
  {
    GtGenomeNode *gn = gt_queue_get(queue);
    gt_genome_node_delete(gn);
  }
  gt_queue_delete(queue);
  
  return agn_unit_test_success(test);
}

static void infer_parent_stream_test_data(GtQueue *queue)
{
  GtError *error = gt_error_new();
  const char *file = "data/gff3/infer-parent-1-in.gff3";
  GtNodeStream *gff3in = gt_gff3_in_stream_new_unsorted(1, &file);
  gt_gff3_in_stream_check_id_attributes((GtGFF3InStream *)gff3in);
  gt_gff3_in_stream_enable_tidy_mode((GtGFF3InStream *)gff3in);

  GtHashmap *type_parents = gt_hashmap_new(GT_HASH_STRING, NULL, NULL);
  gt_hashmap_add(type_parents, "mRNA", "gene");
  gt_hashmap_add(type_parents, "tRNA", "gene");
  GtNodeStream *is = agn_infer_parent_stream_new(gff3in, type_parents);

  GtArray *features = gt_array_new( sizeof(GtGenomeNode *) );
  GtNodeStream *astream = gt_array_out_stream_new(is, features, error);
  int result = gt_node_stream_pull(astream, error);
  if(result == -1)
  {
    fprintf(stderr, "[AgnInferParentStream::infer_parent_stream_test_data] "
            "error processing features: %s\n", gt_error_get(error));
  }

  gt_assert(gt_array_size(features) > 1);
  gt_array_reverse(features);
  while(gt_array_size(features) > 0)
  {
    GtGenomeNode **gn = gt_array_pop(features);
    gt_queue_add(queue, *gn);
  }
  gt_error_delete(error);
  gt_array_delete(features);
  gt_hashmap_delete(type_parents);
  gt_node_stream_delete(astream);
  gt_node_stream_delete(is);
  gt_node_stream_delete(gff3in);
}