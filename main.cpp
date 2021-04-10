#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>

#define DEBUG 1

#if DEBUG
#define assert_stmt(__EXPR, __MSG)             \
  do {                                         \
    if (!(__EXPR)) {                           \
      printf("Assertion failed: %s\n", __MSG); \
    }                                          \
  } while (0)
#else
#define assert_stmt
#endif

// TODO: Wrap in a struct
char *text;
int text_pos = 0;
int text_len;

inline char next_char() {
  ++text_pos;
  return text[text_pos];
}

inline void skip_char() { ++text_pos; }

inline char get_char() { return text[text_pos]; }

inline void consume_char(char ch) {
  if (get_char() == ch) {
    ++text_pos;
    return;
  }
  printf("Expected %c but found %c\n", ch, *text);
  exit(1);
}

inline bool can_be_a_part_of_symbol(char ch) {
  return isalpha(ch) || ch == '+' || ch == '-' || ch == '=' || ch == '-' ||
         ch == '*' || ch == '/' || ch == '>' || ch == '<';
}

enum class ObjType { List, Symbol, String, Number, Nil, Function, Boolean };

static char const *otts[] = {"List", "Symbol",   "String", "Number",
                             "Nil",  "Function", "Boolean"};

int OF_BUILTIN = 0x1;
int OF_LAMBDA = 0x2;
int OF_EVALUATED = 0x4;
int OF_LIST_LITERAL = 0x8;

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

static Object *nil_obj;
static Object *true_obj;
static Object *false_obj;
static Object *dot_obj;
static Object *else_obj;

// Symtable
// TODO: Add limit to the depth of the symbol table (to prevent stack overflows)

using SymVars = std::unordered_map<std::string, Object *>;

struct SymTable {
  SymVars map;
  SymTable *prev;
};

SymTable *symtable;

inline void set_symbol(std::string const &key, Object *value) {
  symtable->map[key] = value;
}
Object *get_symbol(std::string &key) {
  Object *res = nil_obj;
  SymTable *ltable = symtable;
  while (true) {
    bool present = ltable->map.find(key) != ltable->map.end();
    if (present) {
      return ltable->map[key];
    }
    // Global table
    if (ltable->prev == nullptr) {
      return nullptr;
    }
    ltable = ltable->prev;
  }
}
void enter_scope() {
  SymTable *new_scope = new SymTable();
  new_scope->prev = symtable;
  symtable = new_scope;
}
void enter_scope_with(SymVars vars) {
  SymTable *new_scope = new SymTable();
  new_scope->map = vars;
  new_scope->prev = symtable;
  symtable = new_scope;
}
void exit_scope() {
  auto *prev = symtable->prev;
  delete symtable;
  symtable = prev;
}

// Object helpers

char const *obj_type_to_str(ObjType ot) { return otts[(int)ot]; }

Object *new_object(ObjType type, int flags = 0) {
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
bool is_list(Object *obj) { return obj->type == ObjType::List; }
void list_append_inplace(Object *list, Object *item) {
  // we probably need to track dependency of the new list on CDR objects of the
  // argument list so we have to increment ref count for each object. Somehow
  // abstract that into a separate function call? like "ref" for example?
  list->val.l_value->push_back(item);
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
      for (int i = 0; i < a->val.l_value->size(); ++i) {
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

Object *objects_equal(Object *a, Object *b) {
  return bool_obj_from(objects_equal_bare(a, b));
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

Object *objects_gt(Object *a, Object *b) {
  return bool_obj_from(objects_gt_bare(a, b));
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

Object *objects_lt(Object *a, Object *b) {
  return bool_obj_from(objects_lt_bare(a, b));
}

bool is_truthy(Object *obj) {
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

inline char const *fun_name(Object *fun) {
  assert_stmt(fun->type == ObjType::Function,
              "fun_name only accepts functions");
  if (fun->flags & OF_BUILTIN) {
    return fun->val.bf_value.name;
  }
  return list_index(fun->val.f_value.funargs, 0)->val.s_value->data();
}

Object *create_sym_obj(char const *s) {
  auto *res = new_object(ObjType::Symbol);
  res->val.s_value = new std::string(s);
  return res;
}

Object *create_sym_obj(std::string *s) {
  auto *res = new_object(ObjType::Symbol);
  res->val.s_value = s;
  return res;
}

// this is for symbol keywords that don't need to be looked up
template <typename T>
Object *create_final_sym_obj(T s) {
  auto *res = create_sym_obj(s);
  res->flags |= OF_EVALUATED;
  return res;
}

Object *create_num_obj(int v) {
  auto *res = new_object(ObjType::Number, OF_EVALUATED);
  res->val.i_value = v;
  return res;
}

// This function returns a new string representing the object
// You can delete it safely without affecting the object
std::string *obj_to_string_bare(Object *obj) {
  switch (obj->type) {
    case ObjType::String: {
      return new std::string(*obj->val.s_value);
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
      for (int i = 0; i < list_length(obj) - 1; ++i) {
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

Object *obj_to_string(Object *obj) {
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

Object *create_builtin_fobj(char const *name, Builtin handler) {
  Object *res = new_object(ObjType::Function, OF_BUILTIN | OF_EVALUATED);
  res->val.bf_value.builtin_handler = handler;
  res->val.bf_value.name = name;
  return res;
}

////////////////////////////////////////
// Binary operations
////////////////////////////////////////

void error_binop_not_defined(char const *opname, Object const *a,
                             Object const *b) {
  printf("Error: %s operation for objects of type %s and %s is not defined\n",
         opname, obj_type_to_str(a->type), obj_type_to_str(b->type));
}

Object *sub_two_objects(Object *a, Object *b) {
  switch (a->type) {
    case ObjType::Number: {
      if (b->type != ObjType::Number) {
        printf("Can only substract numbers from other numbers\n");
        return nil_obj;
      }
      auto v = a->val.i_value - b->val.i_value;
      return create_num_obj(v);
    } break;
    default: {
      printf("Substraction operation for objects of type %i is not defined\n",
             a->type);
      return nil_obj;
    }
  }
}

Object *add_two_objects(Object *a, Object *b) {
  static char const *opname = "Addition";
  switch (a->type) {
    case ObjType::Number: {
      if (b->type != ObjType::Number) {
        printf("Can only add other numbers to numbers\n");
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

void print_obj(Object *obj, int indent = 0) {
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
      printf("%s[List] %llu: \n", indent_s, obj->val.l_value->size());
      for (int i = 0; i < obj->val.l_value->size(); ++i) {
        auto *lobj = obj->val.l_value->at(i);
        print_obj(lobj, indent + 1);
        printf("\n");
      }
    } break;
    case ObjType::Nil: {
      printf("%s[Nil]", indent_s);
    } break;
    default: {
      printf("Unknown object of type %i\n", obj->type);
    } break;
  }
}

// Parser

Object *read_str() {
  auto *svalue = new std::string("");
  consume_char('"');
  char ch = get_char();
  while (text_pos < text_len && ch != '"') {
    svalue->push_back(ch);
    ch = next_char();
  }
  consume_char('"');
  return create_str_obj(svalue);
}

Object *read_sym() {
  auto *svalue = new std::string("");
  char ch = get_char();
  while (text_pos < text_len && can_be_a_part_of_symbol(ch)) {
    svalue->push_back(ch);
    ch = next_char();
  }
  return create_sym_obj(svalue);
}

Object *read_num() {
  char ch = get_char();
  static char buf[1024];
  int buf_len = 0;
  while (text_pos < text_len && isdigit(ch)) {
    buf[buf_len] = ch;
    ch = next_char();
    ++buf_len;
  }
  buf[buf_len] = '\0';
  int v = std::stoi(buf);
  return create_num_obj(v);
}

Object *read_expr();

Object *read_list(bool literal = false) {
  Object *res = create_list_obj();
  if (literal) {
    res->flags |= OF_LIST_LITERAL;
  }
  consume_char('(');
  while (get_char() != ')') {
    if (text_pos >= text_len) {
      printf("EOF\n");
      exit(1);
      return nullptr;
    }
    auto *e = read_expr();
    list_append_inplace(res, e);
  }
  consume_char(')');
  return res;
}

Object *read_expr() {
  char ch = get_char();
  if (text_pos >= text_len) return nil_obj;
  switch (ch) {
    case ' ':
    case '\n':
    case '\r': {
      // TODO: Count the skipped lines
      skip_char();
      return read_expr();
    } break;
    case ';': {
      // TODO: Count the skipped lines
      // skip until the end of the line
      while (get_char() != '\n') {
        skip_char();
      }
      skip_char();
      return read_expr();
    } break;
    case '(': {
      return read_list();
    } break;
    case '\'': {
      skip_char();
      return read_list(true);
    } break;
    case '"': {
      return read_str();
    } break;
    case '.':
      skip_char();
      return dot_obj;
    default: {
      if (isdigit(ch)) {
        return read_num();
      }
      if (can_be_a_part_of_symbol(ch)) {
        return read_sym();
      }
      printf("Invalid character: %c (%i)\n", ch, ch);
      exit(1);
      return nullptr;
    }
  }
}

Object *eval_expr(Object *expr);

Object *add_objects(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int args_len = elems_len - 1;
  if (args_len < 2) {
    printf("Add (+) operator can't have less than two arguments\n");
    return nil_obj;
  }
  Object *add_res = eval_expr(l->at(1));
  int arg_idx = 2;
  while (arg_idx < elems_len) {
    auto *operand = eval_expr(l->at(arg_idx));
    // actually add objects
    add_res = add_two_objects(add_res, operand);
    ++arg_idx;
  }
  return add_res;
}

Object *sub_objects(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int args_len = elems_len - 1;
  if (args_len < 2) {
    printf("Subtraction (+) operator can't have less than two arguments\n");
    return nil_obj;
  }
  Object *res = eval_expr(l->at(1));
  int arg_idx = 2;
  while (arg_idx < elems_len) {
    auto *operand = eval_expr(l->at(arg_idx));
    res = sub_two_objects(res, operand);
    ++arg_idx;
  }
  return res;
}

////////////////////////////////////////////////////
// Built-ins
////////////////////////////////////////////////////

void create_builtin_function_and_save(char const *name, Builtin handler) {
  set_symbol(name, create_builtin_fobj(name, handler));
}

Object *setq_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int args_len = elems_len - 1;
  if (args_len != 2) {
    printf("setq takes exactly two arguments, %i were given\n", args_len);
    return nil_obj;
  }
  Object *symname = l->at(1);
  Object *symvalue = eval_expr(l->at(2));
  set_symbol(*symname->val.s_value, symvalue);
  return nil_obj;
}

Object *print_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int arg_idx = 1;
  while (arg_idx < elems_len) {
    auto *arg = eval_expr(l->at(arg_idx));
    auto *sobj = obj_to_string(arg);
    // TODO: Handle escape sequences
    printf("%s", sobj->val.s_value->data());
    ++arg_idx;
  }
  printf("\n");
  return nil_obj;
}

Object *defun_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  if (elems_len < 3) {
    printf("Function should have an argument list and a body\n");
    return nil_obj;
  }
  auto *fundef_list = l->at(1);
  // parse function definition list
  if (fundef_list->type != ObjType::List) {
    printf("Function definition list should be a list");
    return nil_obj;
  }
  auto *funobj = new_object(ObjType::Function);
  auto *fundef_list_v = fundef_list->val.l_value;
  auto *funname = fundef_list_v->at(0)->val.s_value;
  funobj->val.f_value.funargs = fundef_list;
  funobj->val.f_value.funbody = expr;
  set_symbol(*funname, funobj);
  return funobj;
}

Object *lambda_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  if (elems_len < 3) {
    printf("Lambdas should have an argument list and a body\n");
    return nil_obj;
  }
  // parse function definition list
  auto *fundef_list = l->at(1);
  if (fundef_list->type != ObjType::List) {
    printf("First paremeter of lambda() should be a list");
    return nil_obj;
  }
  auto *funobj = new_object(ObjType::Function);
  auto *fundef_list_v = fundef_list->val.l_value;
  funobj->flags |= OF_LAMBDA;
  funobj->val.f_value.funargs = fundef_list;
  funobj->val.f_value.funbody = expr;
  return funobj;
}

Object *if_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  if (l->size() != 4) {
    printf(
        "Error: if takes exactly 3 arguments: condition, then, and else "
        "blocks. The function was given %llu arguments instead\n",
        l->size());
    return nil_obj;
  }
  auto *condition = l->at(1);
  auto *then_expr = l->at(2);
  auto *else_expr = l->at(3);
  if (is_truthy(eval_expr(condition))) {
    return eval_expr(then_expr);
  } else {
    return eval_expr(else_expr);
  }
}

Object *binary_builtin(Object *expr, char const *name,
                       BinaryObjOpHandler handler) {
  auto *l = expr->val.l_value;
  size_t given_args = l->size() - 1;
  if (l->size() != 3) {
    printf("%s takes exactly 2 operands, %llu was given\n", name, given_args);
    return nil_obj;
  }
  auto *left_op = eval_expr(l->at(1));
  auto *right_op = eval_expr(l->at(2));
  return handler(left_op, right_op);
}

Object *equal_builtin(Object *expr) {
  return binary_builtin(expr, "=", objects_equal);
}

Object *gt_builtin(Object *expr) {
  return binary_builtin(expr, ">", objects_gt);
}

Object *lt_builtin(Object *expr) {
  return binary_builtin(expr, "<", objects_lt);
}

Object *car_builtin(Object *expr) {
  auto *list_to_operate_on = eval_expr(list_index(expr, 1));
  if (!is_list(list_to_operate_on)) {
    auto *s = obj_to_string_bare(list_to_operate_on);
    printf("car only operates on lists, got %s\n", s->data());
    delete s;
    return nil_obj;
  }
  if (list_length(list_to_operate_on) < 1) return nil_obj;
  return list_index(list_to_operate_on, 0);
}

Object *cadr_builtin(Object *expr) {
  auto *list_to_operate_on = eval_expr(list_index(expr, 1));
  if (!is_list(list_to_operate_on)) {
    auto *s = obj_to_string_bare(list_to_operate_on);
    printf("cadr only operates on lists, got %s\n", s->data());
    delete s;
    return nil_obj;
  }
  if (list_length(list_to_operate_on) < 2) return nil_obj;
  return list_index(list_to_operate_on, 1);
}

Object *cdr_builtin(Object *expr) {
  // currently creating a new list object for every cdr call. Maybe store as a
  // linked list instead and return a pointer to the next of the head so that
  // this call is only O(1)?
  auto *new_list = create_list_obj();
  auto *list_to_operate_on = eval_expr(list_index(expr, 1));
  if (!is_list(list_to_operate_on)) {
    auto *s = obj_to_string_bare(list_to_operate_on);
    printf("cdr only operates on lists, got %s\n", s->data());
    delete s;
    return nil_obj;
  }
  if (list_to_operate_on->type != ObjType::List) {
    printf("cdr can only operate on lists\n");
    return nil_obj;
  }
  if (list_length(list_to_operate_on) < 1) return list_to_operate_on;
  for (int i = 1; i < list_length(list_to_operate_on); ++i) {
    auto *evaluated_item = list_index(list_to_operate_on, i);
    list_append_inplace(new_list, evaluated_item);
  }
  new_list->flags |= OF_EVALUATED;
  return new_list;
}

Object *cond_builtin(Object *expr) {
  // sequentually check every provided condition
  // and if one of them is true, return the provided value
  if (list_length(expr) < 2) {
    printf("cond requires at least one condition pair argument");
    return nil_obj;
  }
  for (int cond_idx = 1; cond_idx < list_length(expr); ++cond_idx) {
    auto *cond_pair = list_index(expr, cond_idx);
    auto *cond_expr = list_index(cond_pair, 0);
    auto *cond_evaluated = eval_expr(cond_expr);
    // this is an "else" branch, and so just return the value since there was no
    // matches before
    bool otherwise_branch = cond_evaluated == else_obj;
    if (otherwise_branch || is_truthy(cond_evaluated)) {
      auto *res = eval_expr(list_index(cond_pair, 1));
      return res;
    }
  }
  return nil_obj;
}

inline void error_builtin_arg_mismatch_function(char const *fname,
                                                size_t expected,
                                                Object const *expr) {
  // 1 for the function name to call
  size_t got = list_length(expr) - 1;
  printf("Error: Built-in %s expected %llu arguments, got %llu\n", fname, expected,
         got);
}

inline bool check_builtin_n_params(char const *bname, Object const *expr,
                                   size_t n) {
  size_t got_params = list_length(expr) - 1;
  if (got_params != n) {
    error_builtin_arg_mismatch_function("timeit", 1, expr);
    return false;
  }
  return true;
}

inline bool check_builtin_no_params(char const *bname, Object const *expr) {
  return check_builtin_n_params(bname, expr, 0);
}

Object *memtotal_builtin(Object *expr) {
  // TODO: Implement
  size_t memtotal = 0;
  auto *s = new std::string(std::to_string(memtotal));
  return create_str_obj(s);
}

Object *timeit_builtin(Object *expr) {
  size_t memtotal = 0;
  if (!check_builtin_n_params("timeit", expr, 1)) return nil_obj;
  auto *expr_to_time = list_index(expr, 1);
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using std::chrono::high_resolution_clock;
  using std::chrono::milliseconds;
  auto start_time = high_resolution_clock::now();
  // discard the result
  eval_expr(expr_to_time);
  auto end_time = high_resolution_clock::now();
  duration<double, std::milli> ms_double = end_time - start_time;
  auto running_time = ms_double.count();
  auto *rtime_s = new std::string(std::to_string(running_time));
  return create_str_obj(rtime_s);
}

Object *sleep_builtin(Object *expr) {
  size_t memtotal = 0;
  if (!check_builtin_n_params("sleep", expr, 1)) return nil_obj;
  auto *ms_num_obj = list_index(expr, 1);
  auto ms = ms_num_obj->val.i_value;
  // sleep the execution thread
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  return nil_obj;
}

Object *call_function(Object *fobj, Object *args_list) {
  // Set arguments in the local scope
  auto *arglistl = fobj->val.f_value.funargs->val.l_value;
  auto *provided_arglistl = args_list->val.l_value;
  bool is_lambda = fobj->flags & OF_LAMBDA;
  // Lambda only have arguments int their arglist, while defuns
  // also have a function name as a first parameter. So we skip that
  // if needed.
  int starting_arg_idx = is_lambda ? 0 : 1;

  SymVars locals;
  auto set_symbol_local = [&](std::string &symname, Object *value) -> bool {
    // evaluate all arguments before calling
    // TODO: Maybe implement lazy evaluation for arguments with context binding?
    auto *evaluated = eval_expr(value);
    locals[symname] = evaluated;
    return true;
  };

  // Because calling function still means that the first element
  // of the list is either a (lambda ()) or a function name (callthis a b c)
  int provided_arg_offset = is_lambda ? 1 : 0;
  for (int arg_idx = starting_arg_idx; arg_idx < arglistl->size(); ++arg_idx) {
    auto *arg = arglistl->at(arg_idx);
    auto *local_arg_name = arg->val.s_value;
    if (arg == dot_obj) {
      // we've reached the end of the usual argument list
      // now variadic arguments start
      // so skip this dot, parse the variadic list arg name, and exit
      if (arg_idx != (arglistl->size() - 2)) {
        // if the dot is not on the pre-last position, print out an error
        // message
        printf(
            "apply (.) operator in function definition incorrectly placed. "
            "It should be at the pre-last position, followed by a vararg "
            "list argument name\n");
        return nil_obj;
      }
      // read all arguments into a list and bind it to the local scope
      auto *varg = arglistl->at(arg_idx + 1);
      auto *varg_lobj = create_data_list_obj();
      for (int provided_arg_idx = arg_idx;
           provided_arg_idx < provided_arglistl->size(); ++provided_arg_idx) {
        auto *provided_arg = provided_arglistl->at(provided_arg_idx);
        // user provided a dot argument, which means that a list containing
        // all the rest of variadic arguments must follow
        if (provided_arg == dot_obj) {
          // the dot must be on the pre-last position
          if (provided_arg_idx != provided_arglistl->size() - 2) {
            auto *fn = fun_name(fobj);
            printf(
                "Error while calling %s: dot notation on the caller side "
                "must be followed by a list argument containing the "
                "variadic expansion list\n",
                fn);
            return nil_obj;
          }
          // expand the rest
          auto *provided_variadic_list =
              eval_expr(provided_arglistl->at(provided_arg_idx + 1));
          if (provided_variadic_list->type != ObjType::List) {
            printf(
                "Error: dot operator on caller side should always be "
                "followed by a list argument\n");
            return nil_obj;
          }
          for (int exp_idx = 0; exp_idx < list_length(provided_variadic_list);
               ++exp_idx) {
            auto *exp_item = list_index(provided_variadic_list, exp_idx);
            list_append_inplace(varg_lobj, exp_item);
          }
          // we're done with the argument list
          break;
        }
        list_append_inplace(varg_lobj, provided_arg);
      }
      set_symbol_local(*varg->val.s_value, varg_lobj);
      break;
    }
    if (arg_idx >= provided_arglistl->size()) {
      // Reached the end of the user-provided argument list, just
      // fill int nils for the remaining arguments
      set_symbol_local(*local_arg_name, nil_obj);
    } else {
      int provided_arg_idx = provided_arg_offset + arg_idx;
      auto *provided_arg = provided_arglistl->at(provided_arg_idx);
      set_symbol_local(*local_arg_name, provided_arg);
    }
  }
  auto *bodyl = fobj->val.f_value.funbody->val.l_value;
  int body_length = bodyl->size();
  // Starting from 1 because 1st index is function name
  int body_expr_idx = 2;
  Object *last_evaluated = nil_obj;
  enter_scope_with(locals);
  while (body_expr_idx < body_length) {
    last_evaluated = eval_expr(bodyl->at(body_expr_idx));
    ++body_expr_idx;
  }
  exit_scope();
  return last_evaluated;
}

bool is_callable(Object *obj) { return obj->type == ObjType::Function; }

Object *eval_expr(Object *expr) {
  if (expr->flags & OF_EVALUATED) {
    return expr;
  }
  switch (expr->type) {
    case ObjType::Symbol: {
      // Look up value of the symbol in the symbol table
      auto *syms = expr->val.s_value;
      auto *res = get_symbol(*syms);
      bool present_in_symtable = res != nullptr;
      if (!present_in_symtable) {
        printf("Symbol not found: \"%s\"\n", syms->data());
        return nil_obj;
      }
      // If object is not yet evaluated
      if (!(res->flags & OF_EVALUATED)) {
        // Evaluate & save in the symbol table
        res = eval_expr(res);
        res->flags |= OF_EVALUATED;
        set_symbol(*syms, res);
      }
      return res;
    } break;
    case ObjType::List: {
      if (expr->flags & OF_LIST_LITERAL) {
        auto *items = list_members(expr);
        for (int i = 0; i < items->size(); ++i) {
          // do we need to evaluate here?
          (*items)[i] = eval_expr(items->at(i));
        }
        expr->flags |= OF_EVALUATED;
        return expr;
      }
      auto *l = expr->val.l_value;
      int elems_len = l->size();
      if (elems_len == 0) return expr;
      auto *op = l->at(0);
      auto *callable = eval_expr(op);
      if (!is_callable(callable)) {
        auto *s = obj_to_string_bare(callable);
        printf("Error: %s is not callable\n", s->data());
        delete s;
        return nil_obj;
      }
      bool is_builtin = callable->flags & OF_BUILTIN;
      if (is_builtin) {
        // Built-in function, no need to do much
        auto *bhandler = callable->val.bf_value.builtin_handler;
        return bhandler(expr);
      }
      // User-defined function
      return call_function(callable, expr);
    }
    default: {
      // For other types (string, number, nil) there is no need to evaluate them
      // as they are in their final form
      return expr;
    } break;
  }
}

char *read_whole_file_into_memory(char const *fp) {
  FILE *f;
  fopen_s(&f, fp, "r");
  if (!f) {
    return nullptr;
  }
  fseek(f, 0L, SEEK_END);
  const size_t file_size = ftell(f);
  fseek(f, 0L, SEEK_SET);
  char *content = (char *)malloc((file_size + 1) * sizeof(*content));
  const size_t READ_N = 128;
  char *content_p = content;
  int read_bytes = 0;
  while (true) {
    read_bytes = fread(content_p, 1, READ_N, f);
    if (read_bytes == 0) {
      if (ferror(f)) {
        printf("Encountered an error while reading the file\n");
        return nullptr;
      }
      *content_p = '\0';
      break;
    }
    content_p += read_bytes;
  }
  return content;
}

#define WIN_32 1

#ifdef WIN_32
char PATH_SEP = '/';
#endif

std::string join_paths(std::string const &a, std::string const &b) {
  std::string res;
  int a_end_pos = a.size() - 1;
  while (a[a_end_pos] == PATH_SEP) {
    --a_end_pos;
  }
  for (int i = 0; i <= a_end_pos; ++i) {
    res += a[i];
  }
  res += PATH_SEP;
  res += b;
  return res;
}

bool load_file(char const *file_to_read) {
  text = read_whole_file_into_memory(file_to_read);
  if (text == nullptr) {
    printf("Couldn't load file at %s, skipping\n", file_to_read);
    return false;
  }
  text_len = strlen(text);
  text_pos = 0;
  while (text_pos < text_len) {
    auto *e = read_expr();
    auto *eval = eval_expr(e);
  }
  return true;
}
void init_interp() {
  // Initialize global symbol table
  symtable = new SymTable();
  symtable->prev = nullptr;
  nil_obj = create_nil_obj();
  true_obj = create_bool_obj(true);
  false_obj = create_bool_obj(false);
  dot_obj = create_final_sym_obj(".");
  else_obj = create_final_sym_obj(".");
  // initialize symtable with builtins
  set_symbol("nil", nil_obj);
  set_symbol("true", true_obj);
  set_symbol("false", false_obj);
  set_symbol("else", else_obj);
  create_builtin_function_and_save("+", (add_objects));
  create_builtin_function_and_save("-", (sub_objects));
  create_builtin_function_and_save("=", (equal_builtin));
  create_builtin_function_and_save(">", (gt_builtin));
  create_builtin_function_and_save("<", (lt_builtin));
  create_builtin_function_and_save("setq", (setq_builtin));
  create_builtin_function_and_save("print", (print_builtin));
  create_builtin_function_and_save("defun", (defun_builtin));
  create_builtin_function_and_save("lambda", (lambda_builtin));
  create_builtin_function_and_save("if", (if_builtin));
  create_builtin_function_and_save("car", (car_builtin));
  create_builtin_function_and_save("cdr", (cdr_builtin));
  create_builtin_function_and_save("cadr", (cadr_builtin));
  create_builtin_function_and_save("cond", (cond_builtin));
  create_builtin_function_and_save("memtotal", (memtotal_builtin));
  create_builtin_function_and_save("timeit", (timeit_builtin));
  create_builtin_function_and_save("sleep", (sleep_builtin));
  // Load the standard library
  char const *STDLIB_PATH = "./stdlib";
  load_file(join_paths(STDLIB_PATH, "basic.lisp").data());
}

void run_interp() {
  std::string input;
  bool is_running = true;
  static std::string prompt = ">> ";
  while (is_running) {
    std::cout << prompt;
    char c;
    while (std::cin.get(c) && c != '\n') {
      input += c;
    }
    if (input == ".exit") {
      is_running = false;
      continue;
    }
    text = input.data();
    text_pos = 0;
    text_len = input.size();
    auto *e = read_expr();
    auto *res = eval_expr(e);
    auto *str_repr = obj_to_string_bare(res);
    std::cout << str_repr->data() << '\n';
    delete str_repr;
    input = "";
  }
}

struct Arguments {
  std::vector<char *> ordered_args;
  bool run_interp = false;
};

Arguments *parse_args(int argc, char **argv) {
  auto *res = new Arguments();
  int argidx = 1;
  while (argidx < argc) {
    char *arg = argv[argidx];
    if (arg[0] == '-') {
      int arg_len = strlen(arg);
      if (arg_len == 1) {
        printf(
            "Argument at position %i is invalid: Dash is followed by nothing",
            argidx);
        return nullptr;
      }
      if (arg[1] == '-') {
        // long named argument
        char *arg_payload = arg + 2;
        if (!strcmp(arg_payload, "interpreter")) {
          res->run_interp = true;
        } else {
          printf("Error: Unknown argument %s\n", arg);
          return nullptr;
        }
      } else {
        // short argument (expect one character)
        if (arg_len != 2) {
          printf(
              "Short arguments starting with a single dash can only be "
              "followed by a single character. Found %s\n",
              arg);
          return nullptr;
        }
        char arg_payload = *(arg + 1);
        switch (arg_payload) {
          case 'i': {
            res->run_interp = true;
          } break;
          default: {
            printf("Error: Unknown argument: %s\n", arg);
            return nullptr;
          } break;
        }
      }
    } else {
      // read ordered argument
      res->ordered_args.push_back(arg);
    }
    ++argidx;
  }
  return res;
}

int main(int argc, char **argv) {
  Arguments *args = parse_args(argc, argv);
  if (args == nullptr) {
    return -1;
  }
  init_interp();
  if (args->run_interp) {
    printf("Running interpreter\n");
    run_interp();
  } else {
    for (auto &file_to_read : args->ordered_args) {
      load_file(file_to_read);
    }
  }
  return 0;
}
