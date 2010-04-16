/*
   alwayszero_plugin.c

   This plugin contains an optimization pass that affects all functions tagged
   as __attribute__((user("alwayszero"))) (typically these functions already
   return 0 yet GCC cannot determine this).

   A statement of the form x = call(); where call is alwayszero will be
   transformed into dummy_var = call(); x = 0;. Further GCC optimization
   passes will eliminate dummy_var and propagate the value 0 into subsequent
   uses of x.

   Ehren Metcalfe
 */

#include "gcc-plugin.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "toplev.h"
#include "basic-block.h"
#include "gimple.h"
#include "tree.h"
#include "tree-pass.h"
#include "intl.h"
#include <string.h>

int plugin_is_GPL_compatible;

/* Attribute handler callback */

static tree
handle_user_attribute (tree *node, tree name, tree args,
                       int flags, bool *no_add_attrs)
{
  return NULL_TREE;
}

/* Attribute definition */

static struct attribute_spec user_attr =
  { "user", 1, 1, false,  false, false, handle_user_attribute };

/* Plugin callback called during attribute registration */

static void
register_attributes (void *event_data, void *data)
{
  register_attribute (&user_attr);
}

/* Checks if stmt is a gimple call tagged with
   __attribute__((user("alwayszero"))) */

static bool
is_alwayszero_function (gimple stmt)
{
  if (!is_gimple_call (stmt))
    return false;

  tree fndecl = gimple_call_fndecl (stmt);
  const char* attrarg = NULL;

  if (fndecl != NULL_TREE)
    {
      tree attrlist = DECL_ATTRIBUTES (fndecl);
      if (attrlist != NULL_TREE)
        {
          tree attr;
          for (attr = lookup_attribute("user", attrlist);
               attr != NULL_TREE;
               attr = lookup_attribute("user", TREE_CHAIN (attr)))
            {
              attrarg = TREE_STRING_POINTER (TREE_VALUE (TREE_VALUE (attr)));
              if (strcmp(attrarg, "alwayszero") == 0)
                return true;
            }
        }
    }
  return false;
}

/* Entry point for the alwayszero optimization pass.
   Creates a new assignment statement with the lhs of the alwayszero call
   then swaps out the old lhs with a new dummy temporary */

static unsigned int
execute_alwayszero_opt (void)
{
  gimple_stmt_iterator gsi;
  basic_block bb;

  FOR_EACH_BB (bb)
    {
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
        {
          gimple stmt = gsi_stmt (gsi);
          if (is_gimple_call (stmt) && gimple_call_lhs (stmt) != NULL_TREE &&
              is_alwayszero_function (stmt))
            {
              tree lhs = gimple_get_lhs (stmt);
              tree zero = build_int_cst (TREE_TYPE (lhs), 0);
              gimple assign = gimple_build_assign_stat (lhs, zero);
              tree var = create_tmp_var (TREE_TYPE (lhs), "dummy_var");
              add_referenced_var (var);
              mark_sym_for_renaming (var);
              gimple_call_set_lhs (stmt, var);
              gsi_insert_after (&gsi, assign, GSI_CONTINUE_LINKING);
            }
        }
    }

  return 0;
}

static struct gimple_opt_pass pass_alwayszero =
{
  {
    GIMPLE_PASS,
    "alwayszero",                         /* name */
    NULL,                                 /* gate */
    execute_alwayszero_opt,               /* execute */
    NULL,                                 /* sub */
    NULL,                                 /* next */
    0,                                    /* static_pass_number */
    0,                                    /* tv_id */
    PROP_cfg | PROP_ssa,                  /* properties_required */
    0,                                    /* properties_provided */
    0,                                    /* properties_destroyed */
    0,                                    /* todo_flags_start */
    TODO_dump_func
      | TODO_verify_ssa
      | TODO_update_ssa                   /* todo_flags_finish */
  }
};

/* The initialization routine exposed to and called by GCC. The spec of this
   function is defined in gcc/gcc-plugin.h. */

int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  struct register_pass_info pass_info;
  const char *plugin_name = plugin_info->base_name;
  int argc = plugin_info->argc;
  struct plugin_argument *argv = plugin_info->argv;

  /* Handle alwayszero functions near conditional constant propagation */
  pass_info.pass = &pass_alwayszero.pass;
  pass_info.reference_pass_name = "ccp";
  pass_info.ref_pass_instance_number = 1;
  /* It seems more logical to insert the pass before ccp, but:
     A) this does the trick anyway, even with regard to dead branch elimination
     B) inserting directly before ccp prevents recognition of user attributes
        for some reason
     C) this pass can go almost anywhere as long as you're in SSA form
   */
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  /* Register this new pass with GCC */
  register_callback (plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                     &pass_info);

  /* Register the user attribute */
  register_callback (plugin_name, PLUGIN_ATTRIBUTES, register_attributes, NULL);

  return 0;
}
