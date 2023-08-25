/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_node_tree_interface_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.h"

#include "WM_types.hh"

const EnumPropertyItem rna_enum_node_tree_interface_item_type_items[] = {
    {NODE_INTERFACE_SOCKET, "SOCKET", 0, "Socket", ""},
    {NODE_INTERFACE_PANEL, "PANEL", 0, "Panel", ""},
    {0, nullptr, 0, nullptr, nullptr}};

#ifdef RNA_RUNTIME

#  include "BKE_node.h"
#  include "BKE_node_runtime.hh"
#  include "BKE_node_tree_interface.hh"
#  include "BKE_node_tree_update.h"
#  include "DNA_material_types.h"
#  include "ED_node.hh"
#  include "WM_api.hh"

/* Internal RNA function declarations, used to invoke registered callbacks. */
extern FunctionRNA rna_NodeTreeInterfaceSocket_draw_func;
extern FunctionRNA rna_NodeTreeInterfaceSocket_init_socket_func;
extern FunctionRNA rna_NodeTreeInterfaceSocket_from_socket_func;

namespace node_interface = blender::bke::node_interface;

static void rna_NodeTreeInterfaceItem_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static StructRNA *rna_NodeTreeInterfaceItem_refine(PointerRNA *ptr)
{
  bNodeTreeInterfaceItem *item = static_cast<bNodeTreeInterfaceItem *>(ptr->data);

  switch (item->item_type) {
    case NODE_INTERFACE_SOCKET: {
      bNodeTreeInterfaceSocket &socket = node_interface::get_item_as<bNodeTreeInterfaceSocket>(
          *item);
      bNodeSocketType *socket_typeinfo = nodeSocketTypeFind(socket.socket_type);
      if (socket_typeinfo && socket_typeinfo->ext_interface.srna) {
        return socket_typeinfo->ext_interface.srna;
      }
      return &RNA_NodeTreeInterfaceSocket;
    }
    case NODE_INTERFACE_PANEL:
      return &RNA_NodeTreeInterfacePanel;
    default:
      return &RNA_NodeTreeInterfaceItem;
  }
}

static char *rna_NodeTreeInterfaceItem_path(const PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  const bNodeTreeInterfaceItem *item = static_cast<const bNodeTreeInterfaceItem *>(ptr->data);
  if (!ntree->runtime) {
    return nullptr;
  }

  ntree->ensure_topology_cache();
  UNUSED_VARS(item);
  // Note: New API, will be enabled after new interface cache is added.
  //  const blender::bke::bNodeTreeInterfaceCache &cache = ntree->interface_cache();
  //  for (const int index : cache.items.index_range()) {
  //    if (cache.items[index] == item) {
  //      return BLI_sprintfN("interface.ui_items[%d]", index);
  //    }
  //  }
  return nullptr;
}

static bool rna_NodeTreeInterfaceSocket_unregister(Main * /*bmain*/, StructRNA *type)
{
  bNodeSocketType *st = static_cast<bNodeSocketType *>(RNA_struct_blender_type_get(type));
  if (!st) {
    return false;
  }

  RNA_struct_free_extension(type, &st->ext_interface);

  RNA_struct_free(&BLENDER_RNA, type);

  /* update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  return true;
}

static void rna_NodeTreeInterfaceSocket_draw_builtin(ID *id,
                                                     bNodeTreeInterfaceSocket *interface_socket,
                                                     bContext *C,
                                                     uiLayout *layout)
{
  bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    UNUSED_VARS(id, C, layout);
    // Note: New API, will be enabled after typeinfo callbacks change.
    //    typeinfo->interface_draw(id, interface_socket, C, layout);
  }
}

// Note: New API, interface draw callback used after changing callbacks.
static void UNUSED_FUNCTION(rna_NodeTreeInterfaceSocket_draw_custom)(
    ID *id, bNodeTreeInterfaceSocket *interface_socket, bContext *C, uiLayout *layout)
{
  bNodeSocketType *typeinfo = nodeSocketTypeFind(interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_NodeTreeInterfaceSocket, interface_socket, &ptr);

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_draw_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  typeinfo->ext_interface.call(C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTreeInterfaceSocket_init_socket_builtin(
    ID *id,
    bNodeTreeInterfaceSocket *interface_socket,
    bNode *node,
    bNodeSocket *socket,
    const char *data_path)
{
  bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    // Note: New API, callback signatures change.
    UNUSED_VARS(id, node, socket, data_path);
    //    typeinfo->interface_init_socket(id, interface_socket, node, socket, data_path);
  }
}

// Note: New API, used when callbacks change.
static void UNUSED_FUNCTION(rna_NodeTreeInterfaceSocket_init_socket_custom)(
    ID *id,
    const bNodeTreeInterfaceSocket *interface_socket,
    bNode *node,
    bNodeSocket *socket,
    const char *data_path)
{
  bNodeSocketType *typeinfo = nodeSocketTypeFind(interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr, node_ptr, socket_ptr;
  RNA_pointer_create(id,
                     &RNA_NodeTreeInterfaceSocket,
                     const_cast<bNodeTreeInterfaceSocket *>(interface_socket),
                     &ptr);
  RNA_pointer_create(id, &RNA_Node, node, &node_ptr);
  RNA_pointer_create(id, &RNA_NodeSocket, socket, &socket_ptr);

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_init_socket_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", &node_ptr);
  RNA_parameter_set_lookup(&list, "socket", &socket_ptr);
  RNA_parameter_set_lookup(&list, "data_path", &data_path);
  typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void rna_NodeTreeInterfaceSocket_from_socket_builtin(
    ID *id, bNodeTreeInterfaceSocket *interface_socket, bNode *node, bNodeSocket *socket)
{
  bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  if (typeinfo && typeinfo->interface_draw) {
    // Note: New API, callback signatures change.
    UNUSED_VARS(id, node, socket);
    //    typeinfo->interface_from_socket(id, interface_socket, node, socket);
  }
}

// Note: New API, used after callback signatures change.
static void UNUSED_FUNCTION(rna_NodeTreeInterfaceSocket_from_socket_custom)(
    ID *id,
    bNodeTreeInterfaceSocket *interface_socket,
    const bNode *node,
    const bNodeSocket *socket)
{
  bNodeSocketType *typeinfo = nodeSocketTypeFind(interface_socket->socket_type);
  if (typeinfo == nullptr) {
    return;
  }

  PointerRNA ptr, node_ptr, socket_ptr;
  RNA_pointer_create(id, &RNA_NodeTreeInterfaceSocket, interface_socket, &ptr);
  RNA_pointer_create(id, &RNA_Node, const_cast<bNode *>(node), &node_ptr);
  RNA_pointer_create(id, &RNA_NodeSocket, const_cast<bNodeSocket *>(socket), &socket_ptr);

  FunctionRNA *func = &rna_NodeTreeInterfaceSocket_from_socket_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", &node_ptr);
  RNA_parameter_set_lookup(&list, "socket", &socket_ptr);
  typeinfo->ext_interface.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static StructRNA *rna_NodeTreeInterfaceSocket_register(Main * /*bmain*/,
                                                       ReportList * /*reports*/,
                                                       void *data,
                                                       const char *identifier,
                                                       StructValidateFunc validate,
                                                       StructCallbackFunc call,
                                                       StructFreeFunc free)
{
  bNodeTreeInterfaceSocket dummy_socket;
  memset(&dummy_socket, 0, sizeof(bNodeTreeInterfaceSocket));
  /* Set #item_type so that refining the type ends up with RNA_NodeTreeInterfaceSocket. */
  dummy_socket.item.item_type = NODE_INTERFACE_SOCKET;

  PointerRNA dummy_socket_ptr;
  RNA_pointer_create(nullptr, &RNA_NodeTreeInterfaceSocket, &dummy_socket, &dummy_socket_ptr);

  /* Validate the python class. */
  bool have_function[3];
  if (validate(&dummy_socket_ptr, data, have_function) != 0) {
    return nullptr;
  }

  /* Check if we have registered this socket type before. */
  bNodeSocketType *st = nodeSocketTypeFind(dummy_socket.socket_type);
  if (st) {
    /* Socket type registered before. */
  }
  else {
    /* Create a new node socket type. */
    st = MEM_cnew<bNodeSocketType>(__func__);
    BLI_strncpy(st->idname, dummy_socket.socket_type, sizeof(st->idname));

    nodeRegisterSocketType(st);
  }

  st->free_self = (void (*)(bNodeSocketType * stype)) MEM_freeN;

  /* if RNA type is already registered, unregister first */
  if (st->ext_interface.srna) {
    StructRNA *srna = st->ext_interface.srna;
    RNA_struct_free_extension(srna, &st->ext_interface);
    RNA_struct_free(&BLENDER_RNA, srna);
  }
  st->ext_interface.srna = RNA_def_struct_ptr(
      &BLENDER_RNA, identifier, &RNA_NodeTreeInterfaceSocket);
  st->ext_interface.data = data;
  st->ext_interface.call = call;
  st->ext_interface.free = free;
  RNA_struct_blender_type_set(st->ext_interface.srna, st);

  // Note: New API callbacks.
  //  st->interface_draw = (have_function[0]) ? rna_NodeTreeInterfaceSocket_draw_custom : nullptr;
  //  st->interface_init_socket = (have_function[1]) ?
  //  rna_NodeTreeInterfaceSocket_init_socket_custom :
  //                                                   nullptr;
  //  st->interface_from_socket = (have_function[2]) ?
  //  rna_NodeTreeInterfaceSocket_from_socket_custom :
  //                                                   nullptr;

  /* Cleanup local dummy type. */
  MEM_SAFE_FREE(dummy_socket.socket_type);

  /* Update while blender is running */
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);

  return st->ext_interface.srna;
}

static IDProperty **rna_NodeTreeInterfaceSocket_idprops(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return &socket->properties;
}

static void rna_NodeTreeInterfaceSocket_identifier_get(PointerRNA *ptr, char *value)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  strcpy(value, socket->identifier);
}

static int rna_NodeTreeInterfaceSocket_identifier_length(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return strlen(socket->identifier);
}

static int rna_NodeTreeInterfaceSocket_socket_type_get(PointerRNA *ptr)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  return rna_node_socket_idname_to_enum(socket->socket_type);
}

static void rna_NodeTreeInterfaceSocket_socket_type_set(PointerRNA *ptr, int value)
{
  bNodeSocketType *typeinfo = rna_node_socket_type_from_enum(value);

  if (typeinfo) {
    bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
    socket->set_socket_type(typeinfo->idname);
  }
}

static bool is_socket_type_supported(bNodeTreeType *ntreetype, bNodeSocketType *socket_type)
{
  /* Check if the node tree supports the socket type. */
  if (ntreetype->valid_socket_type && !ntreetype->valid_socket_type(ntreetype, socket_type)) {
    return false;
  }

  /* Only use basic socket types for this enum. */
  if (socket_type->subtype != PROP_NONE) {
    return false;
  }

  return true;
}

static bNodeSocketType *find_supported_socket_type(bNodeTreeType *ntree_type)
{
  NODE_SOCKET_TYPES_BEGIN (socket_type) {
    if (is_socket_type_supported(ntree_type, socket_type)) {
      return socket_type;
    }
  }
  NODE_SOCKET_TYPES_END;
  return nullptr;
}

static bool rna_NodeTreeInterfaceSocket_socket_type_poll(void *userdata,
                                                         bNodeSocketType *socket_type)
{
  bNodeTreeType *ntreetype = static_cast<bNodeTreeType *>(userdata);
  return is_socket_type_supported(ntreetype, socket_type);
}

static const EnumPropertyItem *rna_NodeTreeInterfaceSocket_socket_type_itemf(
    bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);

  if (!ntree) {
    return rna_enum_dummy_NULL_items;
  }

  return rna_node_socket_type_itemf(
      ntree->typeinfo, rna_NodeTreeInterfaceSocket_socket_type_poll, r_free);
}

static PointerRNA rna_NodeTreeInterfaceItems_active_get(PointerRNA *ptr)
{
  bNodeTreeInterface *interface = static_cast<bNodeTreeInterface *>(ptr->data);
  PointerRNA r_ptr;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeTreeInterfaceItem, interface->active_item(), &r_ptr);
  return r_ptr;
}

static void rna_NodeTreeInterfaceItems_active_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  bNodeTreeInterface *interface = static_cast<bNodeTreeInterface *>(ptr->data);
  bNodeTreeInterfaceItem *item = static_cast<bNodeTreeInterfaceItem *>(value.data);
  interface->active_item_set(item);
}

static bNodeTreeInterfaceSocket *rna_NodeTreeInterfaceItems_new_socket(
    ID *id,
    bNodeTreeInterface *interface,
    Main *bmain,
    ReportList *reports,
    const char *name,
    const char *description,
    bool is_input,
    bool is_output,
    int socket_type_enum,
    bNodeTreeInterfacePanel *parent)
{
  if (parent != nullptr && !interface->find_item(parent->item)) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Parent is not part of the interface");
    return nullptr;
  }
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  bNodeSocketType *typeinfo = rna_node_socket_type_from_enum(socket_type_enum);
  if (typeinfo == nullptr) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Unknown socket type");
    return nullptr;
  }

  /* If data type is unsupported try to find a valid type. */
  if (!is_socket_type_supported(ntree->typeinfo, typeinfo)) {
    typeinfo = find_supported_socket_type(ntree->typeinfo);
    if (typeinfo == nullptr) {
      BKE_report(reports, RPT_ERROR, "Could not find supported socket type");
      return nullptr;
    }
  }
  const char *socket_type = typeinfo->idname;

  eNodeTreeInterfaceSocketFlag flag = eNodeTreeInterfaceSocketFlag(0);
  SET_FLAG_FROM_TEST(flag, is_input, NODE_INTERFACE_SOCKET_INPUT);
  SET_FLAG_FROM_TEST(flag, is_output, NODE_INTERFACE_SOCKET_OUTPUT);

  bNodeTreeInterfaceSocket *socket = interface->add_socket(name ? name : "",
                                                           description ? description : "",
                                                           socket_type ? socket_type : "",
                                                           flag,
                                                           parent);

  if (socket == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create socket");
  }
  else {
    BKE_ntree_update_tag_interface(ntree);
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return socket;
}

static bNodeTreeInterfacePanel *rna_NodeTreeInterfaceItems_new_panel(
    ID *id,
    bNodeTreeInterface *interface,
    Main *bmain,
    ReportList *reports,
    const char *name,
    bNodeTreeInterfacePanel *parent)
{
  if (parent != nullptr && !interface->find_item(parent->item)) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Parent is not part of the interface");
    return nullptr;
  }

  bNodeTreeInterfacePanel *panel = interface->add_panel(name ? name : "", parent);

  if (panel == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create panel");
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    BKE_ntree_update_tag_interface(ntree);
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return panel;
}

static bNodeTreeInterfaceItem *rna_NodeTreeInterfaceItems_copy_to_parent(
    ID *id,
    bNodeTreeInterface *interface,
    Main *bmain,
    ReportList *reports,
    bNodeTreeInterfaceItem *item,
    bNodeTreeInterfacePanel *parent)
{
  if (parent != nullptr && !interface->find_item(parent->item)) {
    BKE_report(reports, RPT_ERROR_INVALID_INPUT, "Parent is not part of the interface");
    return nullptr;
  }

  if (parent == nullptr) {
    parent = &interface->root_panel;
  }
  const int index = parent->items().as_span().first_index_try(item);
  if (!parent->items().index_range().contains(index)) {
    return nullptr;
  }

  bNodeTreeInterfaceItem *item_copy = interface->insert_item_copy(*item, parent, index + 1);

  if (item_copy == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to copy item");
  }
  else {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
    BKE_ntree_update_tag_interface(ntree);
    ED_node_tree_propagate_change(nullptr, bmain, ntree);
    WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
  }

  return item_copy;
}

static bNodeTreeInterfaceItem *rna_NodeTreeInterfaceItems_copy(ID *id,
                                                               bNodeTreeInterface *interface,
                                                               Main *bmain,
                                                               ReportList *reports,
                                                               bNodeTreeInterfaceItem *item)
{
  /* Copy to same parent as the item. */
  bNodeTreeInterfacePanel *parent = interface->find_item_parent(*item);
  if (parent == nullptr) {
    return nullptr;
  }
  return rna_NodeTreeInterfaceItems_copy_to_parent(id, interface, bmain, reports, item, parent);
}

static void rna_NodeTreeInterfaceItems_remove(ID *id,
                                              bNodeTreeInterface *interface,
                                              Main *bmain,
                                              bNodeTreeInterfaceItem *item,
                                              bool move_content_to_parent)
{
  interface->remove_item(*item, move_content_to_parent);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_clear(ID *id, bNodeTreeInterface *interface, Main *bmain)
{
  interface->clear_items();

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_move(
    ID *id, bNodeTreeInterface *interface, Main *bmain, bNodeTreeInterfaceItem *item, int to_index)
{
  interface->move_item(*item, to_index);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTreeInterfaceItems_move_to_parent(ID *id,
                                                      bNodeTreeInterface *interface,
                                                      Main *bmain,
                                                      bNodeTreeInterfaceItem *item,
                                                      bNodeTreeInterfacePanel *parent,
                                                      int to_index)
{
  interface->move_item_to_parent(*item, parent, to_index);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_interface(ntree);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

/* ******** Node Socket Subtypes ******** */

void rna_NodeTreeInterfaceSocketFloat_default_value_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueFloat *dval = static_cast<bNodeSocketValueFloat *>(socket->socket_data);
  bNodeSocketType *socket_typeinfo = nodeSocketTypeFind(socket->socket_type);
  int subtype = socket_typeinfo ? socket_typeinfo->subtype : PROP_NONE;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0.0f : -FLT_MAX);
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

void rna_NodeTreeInterfaceSocketInt_default_value_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueInt *dval = static_cast<bNodeSocketValueInt *>(socket->socket_data);
  bNodeSocketType *socket_typeinfo = nodeSocketTypeFind(socket->socket_type);
  int subtype = socket_typeinfo ? socket_typeinfo->subtype : PROP_NONE;

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = (subtype == PROP_UNSIGNED ? 0 : INT_MIN);
  *max = INT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

void rna_NodeTreeInterfaceSocketVector_default_value_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  bNodeTreeInterfaceSocket *socket = static_cast<bNodeTreeInterfaceSocket *>(ptr->data);
  bNodeSocketValueVector *dval = static_cast<bNodeSocketValueVector *>(socket->socket_data);

  if (dval->max < dval->min) {
    dval->max = dval->min;
  }

  *min = -FLT_MAX;
  *max = FLT_MAX;
  *softmin = dval->min;
  *softmax = dval->max;
}

/* using a context update function here, to avoid searching the node if possible */
static void rna_NodeTreeInterfaceSocket_value_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* default update */
  rna_NodeTreeInterfaceItem_update(bmain, scene, ptr);
}

static bool rna_NodeTreeInterfaceSocketMaterial_default_value_poll(PointerRNA * /*ptr*/,
                                                                   PointerRNA value)
{
  /* Do not show grease pencil materials for now. */
  Material *ma = static_cast<Material *>(value.data);
  return ma->gp_style == nullptr;
}

static void rna_NodeTreeInterface_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return;
  }

  ntree->ensure_topology_cache();
  // Note: New API, enabled when interface cache is added.
  UNUSED_VARS(iter);
  //  const blender::bke::bNodeTreeInterfaceCache &cache = ntree->interface_cache();
  //  rna_iterator_array_begin(iter,
  //                           const_cast<bNodeTreeInterfaceItem **>(cache.items.data()),
  //                           sizeof(bNodeTreeInterfaceItem *),
  //                           cache.items.size(),
  //                           false,
  //                           nullptr);
}

static int rna_NodeTreeInterface_items_length(PointerRNA *ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return 0;
  }

  ntree->ensure_topology_cache();
  // Note: New API callbacks.
  //  return ntree->interface_cache().items.size();
  return 0;
}

static int rna_NodeTreeInterface_items_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return 0;
  }

  ntree->ensure_topology_cache();
  // Note: New API, enabled when interface cache is added.
  UNUSED_VARS(index, r_ptr);
  //  const blender::bke::bNodeTreeInterfaceCache &cache = ntree->interface_cache();
  //  if (!cache.items.index_range().contains(index)) {
  //    return false;
  //  }

  //  RNA_pointer_create(ptr->owner_id, &RNA_NodeTreeInterfaceItem, cache.items[index], r_ptr);
  return true;
}

static int rna_NodeTreeInterface_items_lookup_string(struct PointerRNA *ptr,
                                                     const char *key,
                                                     struct PointerRNA *r_ptr)
{
  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (!ntree->runtime) {
    return 0;
  }

  ntree->ensure_topology_cache();
  // Note: New API, enabled when interface cache is added.
  UNUSED_VARS(key, r_ptr);
  //  const blender::bke::bNodeTreeInterfaceCache &cache = ntree->interface_cache();
  //  for (bNodeTreeInterfaceItem *item : cache.items) {
  //    switch (item->item_type) {
  //      case NODE_INTERFACE_SOCKET: {
  //        bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(item);
  //        if (STREQ(socket->name, key)) {
  //          RNA_pointer_create(ptr->owner_id, &RNA_NodeTreeInterfaceSocket, socket, r_ptr);
  //          return true;
  //        }
  //        break;
  //      }
  //      case NODE_INTERFACE_PANEL: {
  //        bNodeTreeInterfacePanel *panel = reinterpret_cast<bNodeTreeInterfacePanel *>(item);
  //        if (STREQ(panel->name, key)) {
  //          RNA_pointer_create(ptr->owner_id, &RNA_NodeTreeInterfacePanel, panel, r_ptr);
  //          return true;
  //        }
  //        break;
  //      }
  //    }
  //  }
  return false;
}

#else

static void rna_def_node_interface_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterfaceItem", nullptr);
  RNA_def_struct_ui_text(srna, "Node Tree Interface Item", "Item in a node tree interface");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceItem");
  RNA_def_struct_refine_func(srna, "rna_NodeTreeInterfaceItem_refine");
  RNA_def_struct_path_func(srna, "rna_NodeTreeInterfaceItem_path");

  prop = RNA_def_property(srna, "item_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "item_type");
  RNA_def_property_enum_items(prop, rna_enum_node_tree_interface_item_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Item Type", "Type of interface item");
}

static void rna_def_node_interface_socket(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "NodeTreeInterfaceSocket", "NodeTreeInterfaceItem");
  RNA_def_struct_ui_text(srna, "Node Tree Interface Socket", "Declaration of a node socket");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfaceSocket");
  RNA_def_struct_register_funcs(srna,
                                "rna_NodeTreeInterfaceSocket_register",
                                "rna_NodeTreeInterfaceSocket_unregister",
                                nullptr);
  RNA_def_struct_idprops_func(srna, "rna_NodeTreeInterfaceSocket_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Socket name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeTreeInterfaceSocket_identifier_get",
                                "rna_NodeTreeInterfaceSocket_identifier_length",
                                nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_ui_text(prop, "Description", "Socket description");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "socket_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeTreeInterfaceSocket_socket_type_get",
                              "rna_NodeTreeInterfaceSocket_socket_type_set",
                              "rna_NodeTreeInterfaceSocket_socket_type_itemf");
  RNA_def_property_ui_text(
      prop, "Socket Type", "Type of the socket generated by this interface item");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "is_input", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_INPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Is Input", "Whether the socket is an input");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_OUTPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Is Output", "Whether the socket is an output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_HIDE_VALUE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Hide Value", "Hide the socket input value even when the socket is not connected");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "hide_in_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Hide in Modifier",
                           "Don't show the input value in the geometry nodes modifier interface");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_ui_text(
      prop,
      "Attribute Domain",
      "Attribute domain used by the geometry nodes modifier to create an attribute output");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "default_attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "default_attribute_name");
  RNA_def_property_ui_text(prop,
                           "Default Attribute",
                           "The attribute name used by default when the node group is used by a "
                           "geometry nodes modifier");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  /* Registered properties and functions for custom socket types. */
  prop = RNA_def_property(srna, "bl_socket_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "socket_type");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Socket Type Name", "Name of the socket type");

  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Draw properties of the socket interface");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "UILayout");
  RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "init_socket", nullptr);
  RNA_def_function_ui_description(func, "Initialize a node socket instance");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_USE_SELF_ID | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Socket to initialize");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(
      func, "data_path", nullptr, 0, "Data Path", "Path to specialized socket data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "from_socket", nullptr);
  RNA_def_function_ui_description(func, "Setup template parameters from an existing socket");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_USE_SELF_ID | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Original socket");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

static void rna_def_node_interface_panel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterfacePanel", "NodeTreeInterfaceItem");
  RNA_def_struct_ui_text(srna, "Node Tree Interface Item", "Declaration of a node panel");
  RNA_def_struct_sdna(srna, "bNodeTreeInterfacePanel");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Panel name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeTreeInterfaceItem_update");

  prop = RNA_def_property(srna, "interface_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items_array", "items_num");
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Items", "Items in the node panel");
}

static void rna_def_node_tree_interface_items_api(StructRNA *srna)
{
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_index");
  RNA_def_property_ui_text(prop, "Active Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_NODE, nullptr);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_NodeTreeInterfaceItems_active_get",
                                 "rna_NodeTreeInterfaceItems_active_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop, "Active", "Active item");
  RNA_def_property_update(prop, NC_NODE, nullptr);

  func = RNA_def_function(srna, "new_socket", "rna_NodeTreeInterfaceItems_new_socket");
  RNA_def_function_ui_description(func, "Add a new socket to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of the socket");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func, "description", nullptr, 0, "Description", "Description of the socket");
  RNA_def_boolean(func, "is_input", false, "Is Input", "Create an input socket");
  RNA_def_boolean(func, "is_output", false, "Is Output", "Create an output socket");
  parm = RNA_def_enum(func,
                      "socket_type",
                      rna_enum_dummy_DEFAULT_items,
                      0,
                      "Socket Type",
                      "Type of socket generated on nodes");
  /* Note: itemf callback works for the function parameter, it does not require a data pointer. */
  RNA_def_property_enum_funcs(
      parm, nullptr, nullptr, "rna_NodeTreeInterfaceSocket_socket_type_itemf");
  RNA_def_pointer(
      func, "parent", "NodeTreeInterfacePanel", "Parent", "Panel to add the socket in");
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceSocket", "Socket", "New socket");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_panel", "rna_NodeTreeInterfaceItems_new_panel");
  RNA_def_function_ui_description(func, "Add a new panel to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of the new panel");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_pointer(func,
                  "parent",
                  "NodeTreeInterfacePanel",
                  "Parent",
                  "Add panel as a child of the parent panel");
  /* return value */
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfacePanel", "Panel", "New panel");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "copy", "rna_NodeTreeInterfaceItems_copy");
  RNA_def_function_ui_description(func, "Add a copy of an item to the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "Item to copy");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* return value */
  parm = RNA_def_pointer(
      func, "item_copy", "NodeTreeInterfaceItem", "Item Copy", "Copy of the item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NodeTreeInterfaceItems_remove");
  RNA_def_function_ui_description(func, "Remove an item from the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(
      func,
      "move_content_to_parent",
      true,
      "Move Content",
      "If the item is a panel, move the contents to the parent instead of deleting it");

  func = RNA_def_function(srna, "clear", "rna_NodeTreeInterfaceItems_clear");
  RNA_def_function_ui_description(func, "Remove all items from the interface");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);

  func = RNA_def_function(srna, "move", "rna_NodeTreeInterfaceItems_move");
  RNA_def_function_ui_description(func, "Move an item to another position");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the item", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "move_to_parent", "rna_NodeTreeInterfaceItems_move_to_parent");
  RNA_def_function_ui_description(func, "Move an item to a new panel and/or position.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "item", "NodeTreeInterfaceItem", "Item", "The item to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "parent", "NodeTreeInterfacePanel", "Parent", "New parent of the item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the item", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_node_tree_interface(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreeInterface", nullptr);
  RNA_def_struct_ui_text(
      srna, "Node Tree Interface", "Declaration of sockets and ui panels of a node group");
  RNA_def_struct_sdna(srna, "bNodeTreeInterface");

  prop = RNA_def_property(srna, "ui_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_NodeTreeInterface_items_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_NodeTreeInterface_items_length",
                                    "rna_NodeTreeInterface_items_lookup_int",
                                    "rna_NodeTreeInterface_items_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "NodeTreeInterfaceItem");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Items", "Items in the node interface");

  rna_def_node_tree_interface_items_api(srna);
}

void RNA_def_node_tree_interface(BlenderRNA *brna)
{
  rna_def_node_interface_item(brna);
  rna_def_node_interface_socket(brna);
  rna_def_node_interface_panel(brna);
  rna_def_node_tree_interface(brna);

  rna_def_node_socket_interface_subtypes(brna);
}

#endif