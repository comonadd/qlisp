#include "objects.hpp"

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "errors.hpp"
#include "util.hpp"

static char const *otts[] = {"List", "Symbol",   "String", "Number",
                             "Nil",  "Function", "Boolean", "HashTable"};

Object *nil_obj;
Object *true_obj;
Object *false_obj;
Object *dot_obj;
Object *else_obj;

char const *obj_type_to_str(ObjType ot) { return otts[(int)ot]; }

char const *obj_type_s(Object *a) { return obj_type_to_str(a->type); }

Object *sub_two_objects(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      if (b->type != ObjType::Number) {
        error_msg(format(
            "Can only substract numbers from other numbers, got {} and {}",
            obj_type_s(a), obj_type_s(b)));
        return nil_obj;
      }
      auto v = a->val.i_value - b->val.i_value;
      return create_num_obj(v);
    } break;
    default: {
      error_binop_not_defined("Substraction", a, b);
      return nil_obj;
    }
  }
}

Object *add_two_objects(Object *a, Object *b) {
  static char const *opname = "Addition";
  switch (a->type) {
    case ObjType::Number: {
      if (b->type != ObjType::Number) {
        error_msg(
            format("Can only add numbers from other numbers, got {} and {}",
                   obj_type_s(a), obj_type_s(b)));
        return nil_obj;
      }
      auto v = a->val.i_value + b->val.i_value;
      return create_num_obj(v);
    } break;
    case ObjType::String: {
      if (b->type != ObjType::String) {
        error_binop_not_defined(opname, a, b);
        return nil_obj;
      }
      auto *v = new std::string(*a->val.s_value + *b->val.s_value);
      return create_str_obj(v);
    } break;
    default: {
      error_binop_not_defined(opname, a, b);
      return nil_obj;
    }
  }
}

bool objects_equal_bare(Object *a, Object *b) {
  // Objects of different types cannot be equal
  if (a->type != b->type) return false;
  switch (a->type) {
    case ObjType::Number: {
      return a->val.i_value == b->val.i_value;
    } break;
    case ObjType::String: {
      return a->val.s_value == b->val.s_value;
    } break;
    case ObjType::Boolean: {
      return a->val.i_value == b->val.i_value;
    } break;
    case ObjType::List: {
      if (a->val.l_value->size() != b->val.l_value->size()) return false;
      for (size_t i = 0; i < a->val.l_value->size(); ++i) {
        auto *a_member = a->val.l_value->at(i);
        auto *b_member = b->val.l_value->at(i);
        if (!objects_equal_bare(a_member, b_member)) return false;
      }
      return true;
    } break;
    case ObjType::Nil: {
      return true;
    } break;
    case ObjType::Function: {
      // Comparing by argument list memory address for now. Maybe do something
      // else later
      return a->val.f_value.funargs == b->val.f_value.funargs;
    } break;
    default:
      return false;
  }
}

bool objects_gt_bare(Object *a, Object *b) {
  // Objects of different types cannot be compared
  // TODO: Maybe return nil instead?
  if (a->type != b->type) return false_obj;
  switch (a->type) {
    case ObjType::Number: {
      return a->val.i_value > b->val.i_value;
    } break;
    case ObjType::String: {
      return a->val.s_value > b->val.s_value;
    } break;
    case ObjType::Boolean: {
      return a->val.i_value > b->val.i_value;
    } break;
    default:
      return false_obj;
  }
}

bool objects_lt_bare(Object *a, Object *b) {
  // Objects of different types cannot be compared
  // TODO: Maybe return nil instead?
  if (a->type != b->type) return false_obj;
  switch (a->type) {
    case ObjType::Number: {
      return a->val.i_value < b->val.i_value;
    } break;
    case ObjType::String: {
      return a->val.s_value < b->val.s_value;
    } break;
    case ObjType::Boolean: {
      return a->val.i_value < b->val.i_value;
    } break;
    default:
      return false_obj;
  }
}
