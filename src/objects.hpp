#ifndef OBJECTS_HPP
#define OBJECTS_HPP

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "util.hpp"
#include "types.hpp"
#include <fmt/core.h>

using fmt::format;

enum class ObjType { List, Symbol, String, Number, Nil, Function, Boolean };

const int OF_BUILTIN = 0x1;
const int OF_LAMBDA = 0x2;
const int OF_EVALUATED = 0x4;
const int OF_LIST_LITERAL = 0x8;

struct Object;

using Builtin = Object *(*)(Object *);
using BinaryObjOpHandler = Object *(*)(Object *a, Object *b);

struct Object {
  ObjType type;
  int flags = 0;
  union {
    int i_value;
    std::string *s_value;
    std::vector<Object *> *l_value;
    struct {
      char const *name;
      Builtin builtin_handler;
    } bf_value;
    struct {
      Object *funargs;
      Object *funbody;
    } f_value;
  } val;
};

extern Object *nil_obj;
extern Object *true_obj;
extern Object *false_obj;
extern Object *dot_obj;
extern Object *else_obj;

char const *obj_type_to_str(ObjType ot);

inline Object *new_object(ObjType type, int flags = 0) {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = type;
  res->flags = flags;
  return res;
}

// @ROBUSTNESS do we need this ?? we only perform this function call once during
// initialization
inline Object *create_nil_obj() {
  return new_object(ObjType::Nil, OF_EVALUATED);
}

inline Object *create_str_obj(std::string *s) {
  auto *res = new_object(ObjType::String, OF_EVALUATED);
  res->val.s_value = s;
  return res;
}

inline Object *create_bool_obj(bool v) {
  auto *res = new_object(ObjType::Boolean, OF_EVALUATED);
  res->val.i_value = (int)v;
  return res;
}

inline Object *bool_obj_from(bool v) {
  if (v) return true_obj;
  return false_obj;
}

inline Object *create_str_obj(char *cs) {
  auto *res = new_object(ObjType::String);
  res->val.s_value = new std::string(cs);
  return res;
}

inline Object *create_str_obj(int num) {
  auto *num_s = new std::string(std::to_string(num));
  return create_str_obj(num_s);
}

inline Object *create_list_obj() {
  auto *res = new_object(ObjType::List);
  res->val.l_value = new std::vector<Object *>();
  return res;
}
inline Object *create_data_list_obj() {
  auto *res = create_list_obj();
  res->flags |= OF_EVALUATED;
  return res;
}
inline size_t list_length(Object const *list) {
  return list->val.l_value->size();
}
inline Object *list_index(Object *list, size_t i) {
  return list->val.l_value->at(i);
}
inline std::vector<Object *> *list_members(Object *list) {
  return list->val.l_value;
}
inline bool is_list(Object *obj) { return obj->type == ObjType::List; }
inline void list_append_inplace(Object *list, Object *item) {
  // we probably need to track dependency of the new list on CDR objects of the
  // argument list so we have to increment ref count for each object. Somehow
  // abstract that into a separate function call? like "ref" for example?
  list->val.l_value->push_back(item);
}
inline void list_append_list_inplace(Object *list, Object *to_append) {
  if (to_append->type != ObjType::List) {
    list_append_inplace(list, to_append);
    return;
  }
  for (auto* item : *list_members(to_append)) {
    list_append_inplace(list, item);
  }
}

inline char const *fun_name(Object *fun) {
  assert_stmt(fun->type == ObjType::Function,
              "fun_name only accepts functions");
  if (fun->flags & OF_BUILTIN) {
    return fun->val.bf_value.name;
  }
  return list_index(fun->val.f_value.funargs, 0)->val.s_value->data();
}

inline Object *create_sym_obj(char const *s) {
  auto *res = new_object(ObjType::Symbol);
  res->val.s_value = new std::string(s);
  return res;
}

inline Object *create_sym_obj(std::string *s) {
  auto *res = new_object(ObjType::Symbol);
  res->val.s_value = s;
  return res;
}

// this is for symbol keywords that don't need to be looked up
template <typename T>
inline Object *create_final_sym_obj(T s) {
  auto *res = create_sym_obj(s);
  res->flags |= OF_EVALUATED;
  return res;
}

inline Object *create_num_obj(int v) {
  auto *res = new_object(ObjType::Number, OF_EVALUATED);
  res->val.i_value = v;
  return res;
}

inline bool is_truthy(Object *obj) {
  switch (obj->type) {
    case ObjType::Boolean: {
      return obj->val.i_value != 0;
    } break;
    case ObjType::Number: {
      return obj->val.i_value != 0;
    } break;
    case ObjType::String: {
      return obj->val.s_value->size() != 0;
    } break;
    case ObjType::List: {
      return obj->val.l_value->size() != 0;
    } break;
    case ObjType::Nil: {
      return false;
    } break;
    case ObjType::Function: {
      return true;
    } break;
    default: {
      return false;
    } break;
  }
}

// This function returns a new string representing the object
// You can delete it safely without affecting the object
inline std::string *obj_to_string_bare(Object *obj) {
  switch (obj->type) {
    case ObjType::String: {
      return new std::string(*obj->val.s_value);
    } break;
    case ObjType::Symbol: {
      auto* res = new std::string("[Symbol \"");
      *res += *obj->val.s_value;
      *res += "\"]";
      return res;
    } break;
    case ObjType::Number: {
      auto *s = new std::string(std::to_string(obj->val.i_value));
      return s;
    } break;
    case ObjType::Function: {
      auto const *fn = fun_name(obj);
      std::string *s = new std::string("[Function ");
      if (obj->flags & OF_BUILTIN) {
        *s += "(builtin) ";
      }
      *s += fn;
      *s += ']';
      return s;
    } break;
    case ObjType::List: {
      auto *res = new std::string("(");
      for (size_t i = 0; i < list_length(obj) - 1; ++i) {
        auto *member = list_index(obj, i);
        *res += *obj_to_string_bare(member);
        *res += ' ';
      }
      *res += *obj_to_string_bare(list_index(obj, list_length(obj) - 1));
      *res += ')';
      return res;
    } break;
    case ObjType::Boolean: {
      if (obj == false_obj) return new std::string("false");
      if (obj == true_obj) return new std::string("true");
      assert_stmt(false, "Impossible case");
      return nullptr;
    } break;
    default: {
      return new std::string("nil");
    }
  }
}

inline Object *obj_to_string(Object *obj) {
  // TODO: Implement for symbols
  switch (obj->type) {
    case ObjType::String: {
      return obj;
    } break;
    default: {
      auto s = obj_to_string_bare(obj);
      return create_str_obj(s);
    } break;
  }
}

inline Object *create_builtin_fobj(char const *name, Builtin handler) {
  Object *res = new_object(ObjType::Function, OF_BUILTIN | OF_EVALUATED);
  res->val.bf_value.builtin_handler = handler;
  res->val.bf_value.name = name;
  return res;
}

inline void print_obj(Object *obj, int indent = 0) {
  char indent_s[16];
  memset(indent_s, ' ', indent);
  indent_s[indent] = '\0';
  switch (obj->type) {
    case ObjType::Number: {
      printf("%s[Num] %i", indent_s, obj->val.i_value);
    } break;
    case ObjType::String: {
      printf("%s[Str] %s", indent_s, obj->val.s_value->data());
    } break;
    case ObjType::Symbol: {
      printf("%s[Sym] %s", indent_s, obj->val.s_value->data());
    } break;
    case ObjType::Function: {
      if (obj->flags & OF_BUILTIN) {
        auto *funname = obj->val.bf_value.name;
        printf("%s[Builtin] %s\n", indent_s, funname);
      } else {
        auto fval = obj->val.f_value;
        auto *funname = fval.funargs->val.l_value->at(0)->val.s_value;
        printf("%s[Function] %s\n", indent_s, funname->data());
      }
    } break;
    case ObjType::List: {
      printf("%s[List] %lu: \n", indent_s, obj->val.l_value->size());
      for (size_t i = 0; i < obj->val.l_value->size(); ++i) {
        auto *lobj = obj->val.l_value->at(i);
        print_obj(lobj, indent + 1);
        printf("\n");
      }
    } break;
    case ObjType::Nil: {
      printf("%s[Nil]", indent_s);
    } break;
    default: {
      printf("Unknown object of type %s\n", obj_type_to_str(obj->type));
    } break;
  }
}

////////////////////////////////////////
// Binary operations
////////////////////////////////////////

inline void error_binop_not_defined(char const *opname, Object const *a,
                                    Object const *b) {
  printf("Error: %s operation for objects of type %s and %s is not defined\n",
         opname, obj_type_to_str(a->type), obj_type_to_str(b->type));
}

Object *sub_two_objects(Object *a, Object *b);

Object *add_two_objects(Object *a, Object *b);

bool objects_equal_bare(Object *a, Object *b);

inline Object *objects_equal(Object *a, Object *b) {
  return bool_obj_from(objects_equal_bare(a, b));
}

bool objects_gt_bare(Object *a, Object *b);

inline Object *objects_gt(Object *a, Object *b) {
  return bool_obj_from(objects_gt_bare(a, b));
}

bool objects_lt_bare(Object *a, Object *b);

inline Object *objects_lt(Object *a, Object *b) {
  return bool_obj_from(objects_lt_bare(a, b));
}

inline Object *objects_div(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      auto val = a->val.i_value / b->val.i_value;
      return create_num_obj(val);
    } break;
    default: {
      error_binop_not_defined("Division", a, b);
      return nil_obj;
    } break;
  }
}

inline Object *objects_pow(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      auto val = pow(a->val.i_value, b->val.i_value);
      return create_num_obj(val);
    } break;
    default: {
      error_binop_not_defined("Power", a, b);
      return nil_obj;
    } break;
  }
}

inline Object *objects_mul(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      auto val = a->val.i_value * b->val.i_value;
      return create_num_obj(val);
    } break;
    default: {
      error_binop_not_defined("Multiplication", a, b);
      return nil_obj;
    } break;
  }
}

inline Object *objects_rem(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      i32 val = a->val.i_value % b->val.i_value;
      return create_num_obj(val);
    } break;
    default: {
      error_binop_not_defined("Remainder", a, b);
      return nil_obj;
    } break;
  }
}

#endif
