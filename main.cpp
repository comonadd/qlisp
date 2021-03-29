#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>

#define DEBUG 1

#if DEBUG
#define assert_stmt(__EXPR, __MSG)                                             \
  do {                                                                         \
    if (!(__EXPR)) {                                                           \
      printf("Assertion failed: %s\n", __MSG);                                 \
    }                                                                          \
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

int OF_BUILTIN = 0x1;
int OF_LAMBDA = 0x2;
int OF_EVALUATED = 0x4;

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
      char *name;
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

// Symtable

struct SymTable {
  std::unordered_map<std::string, Object *> map;
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
      return nil_obj;
    }
    ltable = ltable->prev;
  }
}
void enter_scope() {
  SymTable *new_scope = new SymTable();
  new_scope->prev = symtable;
  symtable = new_scope;
}
void exit_scope() {
  auto *prev = symtable->prev;
  delete symtable;
  symtable = prev;
}

// Object helpers

Object *new_object(ObjType type, int flags = 0) {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = type;
  res->flags = flags;
  return res;
}

Object *create_nil_obj() { return new_object(ObjType::Nil, OF_EVALUATED); }

Object *create_str_obj(std::string *s) {
  auto *res = new_object(ObjType::String, OF_EVALUATED);
  res->val.s_value = s;
  return res;
}

Object *create_bool_obj(bool v) {
  auto *res = new_object(ObjType::Boolean, OF_EVALUATED);
  res->val.i_value = (int)v;
  return res;
}

Object *bool_obj_from(bool v) {
  if (v)
    return true_obj;
  return false_obj;
}

Object *create_sym_obj(std::string *s) {
  auto *res = new_object(ObjType::Symbol);
  res->val.s_value = s;
  return res;
}

Object *create_str_obj(char *cs) {
  auto *res = new_object(ObjType::String);
  res->val.s_value = new std::string(cs);
  return res;
}

Object *create_str_obj(int num) {
  auto *num_s = new std::string(std::to_string(num));
  return create_str_obj(num_s);
}

Object *create_list_obj() {
  auto *res = new_object(ObjType::List);
  res->val.l_value = new std::vector<Object *>();
  return res;
}

Object *create_num_obj(int v) {
  auto *res = new_object(ObjType::Number, OF_EVALUATED);
  res->val.i_value = v;
  return res;
}

void list_append_inplace(Object *list, Object *item) {
  list->val.l_value->push_back(item);
}

Object *create_builtin_fobj(char *name, Builtin handler) {
  Object *res = new_object(ObjType::Function, OF_BUILTIN | OF_EVALUATED);
  res->val.bf_value.builtin_handler = handler;
  res->val.bf_value.name = name;
  return res;
}

void create_builtin_function_and_save(char *name, Builtin handler) {
  set_symbol(name, create_builtin_fobj(name, handler));
}

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

Object *read_list() {
  Object *res = create_list_obj();
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
  if (text_pos >= text_len)
    return nil_obj;
  switch (ch) {
  case ' ':
  case '\n':
  case '\r': {
    skip_char();
    return read_expr();
  } break;
  case '(': {
    return read_list();
  } break;
  case '\'': {
    return nullptr;
  } break;
  case '"': {
    return read_str();
  } break;
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

Object *eval_expr(Object *expr);

Object *add_two_objects(Object *a, Object *b) {
  switch (a->type) {
  case ObjType::Number: {
    if (b->type != ObjType::Number) {
      printf("Can only add other numbers to numbers\n");
      return nil_obj;
    }
    auto v = a->val.i_value + b->val.i_value;
    return create_num_obj(v);
  } break;
  default: {
    printf("Addition operation for objects of type %i is not defined\n",
           a->type);
    return nil_obj;
  }
  }
}

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

Object *obj_to_string(Object *obj) {
  // TODO: Implement for symbols
  switch (obj->type) {
  case ObjType::String: {
    return obj;
  } break;
  case ObjType::Number: {
    return create_str_obj(obj->val.i_value);
  } break;
  case ObjType::Nil: {
    return create_str_obj("nil");
  } break;
  default: {
    return create_str_obj("nil");
  }
  }
}

Object *print_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int arg_idx = 1;
  while (arg_idx < elems_len) {
    auto *arg = eval_expr(l->at(arg_idx));
    auto *sobj = obj_to_string(arg);
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

Object *if_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  if (l->size() != 4) {
    printf("if takes exactly 3 arguments: condition, then, and else blocks. "
           "The function was given %i arguments instead\n",
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

bool objects_equal_bare(Object *a, Object *b) {
  // Objects of different types cannot be equal
  if (a->type != b->type)
    return false_obj;
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
    if (a->val.l_value->size() != b->val.l_value->size())
      return false;
    for (int i = 0; i < a->val.l_value->size(); ++i) {
      auto *a_member = a->val.l_value->at(i);
      auto *b_member = b->val.l_value->at(i);
      if (!objects_equal_bare(a_member, b_member))
        return false;
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
    return false_obj;
  }
}

Object *objects_equal(Object *a, Object *b) {
  return bool_obj_from(objects_equal_bare(a, b));
}

bool objects_gt_bare(Object *a, Object *b) {
  // Objects of different types cannot be compared
  // TODO: Maybe return nil instead?
  if (a->type != b->type)
    return false_obj;
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
  if (a->type != b->type)
    return false_obj;
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

Object *binary_builtin(Object *expr, char const *name,
                       BinaryObjOpHandler handler) {
  auto *l = expr->val.l_value;
  if (l->size() != 3) {
    printf("%s takes exactly 2 operands, %i was given\n", name, l->size());
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

Object *call_function(Object *fobj, Object *args_list) {
  enter_scope();
  // Set arguments in the local scope
  auto *arglistl = fobj->val.f_value.funargs->val.l_value;
  auto *provided_arglistl = args_list->val.l_value;
  bool is_lambda = fobj->flags & OF_LAMBDA;
  // Lambda only have arguments int their arglist, while defuns
  // also have a function name as a first parameter. So we skip that
  // if needed.
  int starting_arg_idx = is_lambda ? 0 : 1;
  // Because calling function still means that the first element
  // of the list is either a (lambda ()) or a function name (callthis a b c)
  int provided_arg_offset = is_lambda ? 1 : 0;
  for (int arg_idx = starting_arg_idx; arg_idx < arglistl->size(); ++arg_idx) {
    auto *arg = arglistl->at(arg_idx);
    auto *local_arg_name = arg->val.s_value;
    if (arg_idx >= provided_arglistl->size()) {
      // Reached the end of the user-provided argument list, just
      // fill int nils for the remaining arguments
      set_symbol(*local_arg_name, nil_obj);
    } else {
      int provided_arg_idx = provided_arg_offset + arg_idx;
      auto *provided_arg = provided_arglistl->at(provided_arg_idx);
      set_symbol(*local_arg_name, provided_arg);
    }
  }
  auto *bodyl = fobj->val.f_value.funbody->val.l_value;
  int body_length = bodyl->size();
  // Starting from 1 because 1st index is function name
  int body_expr_idx = 2;
  Object *last_evaluated = nil_obj;
  while (body_expr_idx < body_length) {
    last_evaluated = eval_expr(bodyl->at(body_expr_idx));
    ++body_expr_idx;
  }
  exit_scope();
  return last_evaluated;
}

Object *eval_expr(Object *expr) {
  if (expr->flags & OF_EVALUATED) {
    return expr;
  }
  switch (expr->type) {
  case ObjType::Symbol: {
    // Look up value of the symbol in the symbol table
    auto *syms = expr->val.s_value;
    auto *res = get_symbol(*syms);
    bool present_in_symtable = res != nil_obj;
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
    auto *l = expr->val.l_value;
    int elems_len = l->size();
    if (elems_len == 0)
      return expr;
    auto *op = l->at(0);
    auto *callable = eval_expr(op);
    if (callable == nil_obj) {
      printf("Nil is not callable\n");
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
  FILE *f = fopen(fp, "r");
  if (!f) {
    printf("Failed to open file at %s\n", fp);
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

void init_interp() {
  // Initialize global symbol table
  symtable = new SymTable();
  symtable->prev = nullptr;
  nil_obj = create_nil_obj();
  true_obj = create_bool_obj(true);
  false_obj = create_bool_obj(false);
  // initialize symtable with builtins
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
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("You should provide at least one file path to execute\n");
    return -1;
  }
  init_interp();
  for (int i = 1; i < argc; ++i) {
    char *file_to_read = argv[i];
    text = read_whole_file_into_memory(file_to_read);
    if (text == nullptr) {
      printf("Couldn't load file at %s, skipping\n", file_to_read);
      continue;
    }
    text_len = strlen(text);
    while (text_pos < text_len) {
      auto *e = read_expr();
      // printf("Expression-------------------------------------------\n");
      // print_obj(e);
      // printf("\n");
      // printf("Evaluated expression---------------------------------\n");
      auto *eval = eval_expr(e);
      // print_obj(eval);
      // printf("\n");
    }
  }
  return 0;
}
