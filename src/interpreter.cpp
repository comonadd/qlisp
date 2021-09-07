#include "interpreter.hpp"

#include <fmt/core.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "errors.hpp"
#include "objects.hpp"
#include "platform/platform.hpp"
#include "util.hpp"

using fmt::format;
using std::filesystem::path;

InterpreterState IS;
GarbageCollector GC;

inline bool can_start_a_symbol(char ch) {
  return isalpha(ch) || ch == '+' || ch == '-' || ch == '=' || ch == '-' ||
         ch == '*' || ch == '/' || ch == '>' || ch == '<' || ch == '?';
}

inline bool can_be_a_part_of_symbol(char ch) {
  return isalnum(ch) || ch == '+' || ch == '-' || ch == '=' || ch == '-' ||
         ch == '*' || ch == '/' || ch == '>' || ch == '<' || ch == '?';
}

inline char next_char() {
  ++IS.text_pos;
  return IS.text[IS.text_pos];
}

inline char get_char() { return IS.text[IS.text_pos]; }

inline void skip_char() {
  ++IS.col;
  ++IS.text_pos;
}

inline void consume_char(char ch) {
  ++IS.col;
  if (get_char() == ch) {
    ++IS.text_pos;
    return;
  }
  error_msg(format("Expected {} but found {}\n", ch, *IS.text));
}

inline void set_symbol(std::string const &key, Object *value) {
  inc_ref(value);
  IS.symtable->map[key] = value;
}

Object *get_symbol(std::string &key) {
  SymTable *ltable = IS.symtable;
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

// TODO: Add limit to the depth of the symbol table (to prevent stack overflows)
void enter_scope() {
  SymTable *new_scope = new SymTable();
  new_scope->prev = IS.symtable;
  IS.symtable = new_scope;
}

void enter_scope_with(SymVars vars) {
  SymTable *new_scope = new SymTable();
  new_scope->map = vars;
  for (auto &var : new_scope->map) {
    inc_ref(var.second);
  }
  new_scope->prev = IS.symtable;
  IS.symtable = new_scope;
}

void exit_scope() {
  assert_stmt(IS.symtable->prev != nullptr, "Trying to exit global scope");
  auto *prev = IS.symtable->prev;
  // decrease references to all referenced objects in scope
  for (auto &s : IS.symtable->map) {
    dec_ref(s.second);
  }
  delete IS.symtable;
  IS.symtable = prev;
}

Object *read_str() {
  auto *svalue = new std::string("");
  consume_char('"');
  char ch = get_char();
  while (IS.text_pos < IS.text_len) {
    if (ch == '\\') {
      ch = next_char();
      if (IS.text_pos >= IS.text_len) {
        eof_error();
        delete svalue;
        return nil_obj;
      }
      switch (ch) {
        case 'n': {
          svalue->push_back('\n');
        } break;
        case 'r': {
          svalue->push_back('\r');
        } break;
        case '0': {
          svalue->push_back('\0');
        } break;
        case '"': {
          svalue->push_back('"');
        } break;
        case 't': {
          svalue->push_back('\t');
        } break;
        case '\\': {
          svalue->push_back('\\');
        } break;
        default: {
          error_msg(format("Invalid escape sequence: \"\{}\"", ch));
        } break;
      }
      ch = next_char();
      continue;
    } else if (ch == '"') {
      break;
    } else {
      svalue->push_back(ch);
      ch = next_char();
    }
  }
  consume_char('"');
  return create_str_obj(svalue);
}

Object *read_sym() {
  auto *svalue = new std::string("");
  char ch = get_char();
  while (IS.text_pos < IS.text_len && can_be_a_part_of_symbol(ch)) {
    svalue->push_back(ch);
    ch = next_char();
  }
  return create_sym_obj(svalue);
}

Object *read_num() {
  char ch = get_char();
  static char buf[1024];
  int buf_len = 0;
  while (IS.text_pos < IS.text_len && isdigit(ch)) {
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
    if (IS.text_pos >= IS.text_len) {
      eof_error();
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
  if (IS.text_pos >= IS.text_len) return nil_obj;
  switch (ch) {
    case ' ': {
      skip_char();
      return read_expr();
    } break;
    case '\n':
    case '\r': {
      // TODO: Count the skipped lines
      skip_char();
      ++IS.line;
      IS.col = 0;
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
    case '.': {
      skip_char();
      return dot_obj;
    } break;
    case '\0': {
      skip_char();
      return nil_obj;
    } break;
    default: {
      if (isdigit(ch)) {
        return read_num();
      }
      if (can_start_a_symbol(ch)) {
        return read_sym();
      }
      error_msg(format("Invalid character: {} ({:d})", ch, ch));
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

Object *begin_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  int elems_len = l->size();
  int arg_idx = 1;
  Object *last_evaluated = nil_obj;
  while (arg_idx < elems_len) {
    auto *arg = eval_expr(l->at(arg_idx));
    last_evaluated = arg;
    ++arg_idx;
  }
  return last_evaluated;
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
    error_msg(format("First paremeter of lambda() should be a list, got \"{}\"",
                     obj_type_to_str(fundef_list->type)));
    return nil_obj;
  }
  auto *funobj = new_object(ObjType::Function);
  funobj->flags |= OF_LAMBDA;
  funobj->val.f_value.funargs = fundef_list;
  funobj->val.f_value.funbody = expr;
  return funobj;
}

Object *eval_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  u32 elems_len = l->size();
  if (elems_len < 2) {
    error_msg(format(
        "eval needs at least one expression to evaluate as an argument, got {}",
        elems_len));
    return nil_obj;
  }
  Object *res = nil_obj;
  auto saved_is = IS;
  for (u32 i = 1; i < elems_len; ++i) {
    auto *expr_obj = eval_expr(l->at(i));
    if (expr_obj->type != ObjType::String) {
      error_msg(format("Eval can only evaluate strings, got \"{}\"",
                       obj_type_to_str(expr_obj->type)));
      res = nil_obj;
      break;
    }
    IS.line = 1;
    IS.col = 0;
    IS.text = expr_obj->val.s_value->c_str();
    IS.text_pos = 0;
    Object *e = read_expr();
    res = eval_expr(e);
  }
  IS = saved_is;
  return res;
}

Object *if_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  if (l->size() != 4) {
    error_msg(
        format("if takes exactly 3 arguments: condition, then, and else "
               "blocks. The function was given {} arguments instead\n",
               l->size()));
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
    error_msg(format("{} takes exactly 2 operands, {} was given\n", name,
                     given_args));
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

Object *not_builtin(Object *expr) {
  auto *l = expr->val.l_value;
  if (l->size() != 2) {
    error_msg(
        format("not takes exactly 1 argument, {} was given\n", l->size()));
    return nil_obj;
  }
  auto *operand = eval_expr(l->at(1));
  if (is_truthy(operand)) return false_obj;
  return true_obj;
}

Object *div_objects_builtin(Object *expr) {
  return binary_builtin(expr, "/", objects_div);
}

Object *div_objects_rem(Object *expr) {
  return binary_builtin(expr, "remainder", objects_rem);
}

Object *mul_objects_builtin(Object *expr) {
  return binary_builtin(expr, "*", objects_mul);
}

Object *pow_objects_builtin(Object *expr) {
  return binary_builtin(expr, "**", objects_pow);
}

Object *car_builtin(Object *expr) {
  auto *list_to_operate_on = eval_expr(list_index(expr, 1));
  if (!is_list(list_to_operate_on)) {
    auto *s = obj_to_string_bare(list_to_operate_on);
    error_msg(format("car only operates on lists, got {}\n", s->data()));
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
  for (size_t i = 1; i < list_length(list_to_operate_on); ++i) {
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
    error_msg("cond requires at least one condition pair argument");
    return nil_obj;
  }
  for (size_t cond_idx = 1; cond_idx < list_length(expr); ++cond_idx) {
    auto *cond_pair = list_index(expr, cond_idx);
    auto *cond_expr = list_index(cond_pair, 0);
    auto *cond_evaluated = eval_expr(cond_expr);
    // this is an "else" branch, and so just return the value since there was no
    // matches before
    bool otherwise_branch = cond_evaluated == else_obj;
    if (otherwise_branch || is_truthy(cond_evaluated)) {
      Object *res = nil_obj;
      for (size_t i = 1; i < list_length(cond_pair); ++i) {
        res = eval_expr(list_index(cond_pair, i));
      }
      return res;
    }
  }
  return nil_obj;
}

Object *let_builtin(Object *expr) {
  if (list_length(expr) != 3) {
    error_msg("let requires exactly two arguments");
    return nil_obj;
  }
  enter_scope();
  auto *bindings = list_index(expr, 1);
  for (size_t idx = 0; idx < list_length(bindings); ++idx) {
    auto *let_pair = list_index(bindings, idx);
    if (let_pair->type != ObjType::List) {
      error_msg(format("let binding list should consist of lists, got \"{}\"",
                       obj_type_to_str(let_pair->type)));
      break;
    }
    auto *let_name = list_index(let_pair, 0);
    auto *let_value = eval_expr(list_index(let_pair, 1));
    if (let_name->type != ObjType::Symbol) {
      error_msg(format("let binding name must be a symbol, got \"{}\"",
                       obj_type_to_str(let_name->type)));
      break;
    }
    set_symbol(*let_name->val.s_value, let_value);
  }
  auto *let_body = list_index(expr, 2);
  auto *res = eval_expr(let_body);
  exit_scope();
  return res;
}

Object *cons_builtin(Object *expr) {
  if (list_length(expr) < 2) {
    error_msg("cons requires at least one condition pair argument");
    return nil_obj;
  }
  auto *res = create_data_list_obj();
  for (size_t idx = 1; idx < list_length(expr); ++idx) {
    auto *lexpr = list_index(expr, idx);
    auto *l = eval_expr(lexpr);
    list_append_list_inplace(res, l);
  }
  return res;
}

enum class EA {
  LEQ,
  GEQ,
  EQ,
};

bool expect_args_check(Object const *builtin_expr, std::string const &name,
                       EA k, u32 n) {
  u64 num_args_given = list_length(builtin_expr) - 1;
  switch (k) {
    case EA::GEQ: {
      // expect more than n arguments
      if (num_args_given < n) {
        error_msg(format("\"{}\" expects at least {} arguments, {} was given",
                         name, n, num_args_given));
        return false;
      }
    } break;
    case EA::LEQ: {
      // expect more than n arguments
      if (num_args_given > n) {
        error_msg(format("\"{}\" expects at most {} arguments, {} was given",
                         name, n, num_args_given));
        return false;
      }
    } break;
    case EA::EQ: {
      // expect exactly n arguments
      if (num_args_given != n) {
        error_msg(format("\"{}\" expects exactly {} arguments, {} was given",
                         name, n, num_args_given));
        return false;
      }
    } break;
  }
  return true;
}

inline bool check_builtin_n_params(char const *bname, Object const *expr,
                                   size_t n) {
  return expect_args_check(expr, bname, EA::EQ, n);
}

inline bool check_builtin_no_params(char const *bname, Object const *expr) {
  return check_builtin_n_params(bname, expr, 0);
}

Object *memtotal_builtin(Object *expr) {
  size_t memtotal = get_total_memory_usage();
  return create_num_obj(memtotal);
}

Object *timeit_builtin(Object *expr) {
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
  if (!check_builtin_n_params("sleep", expr, 1)) return nil_obj;
  auto *ms_num_obj = list_index(expr, 1);
  auto ms = ms_num_obj->val.i_value;
  // sleep the execution thread
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  return nil_obj;
}

bool expect_arg_type(Object *expr, std::string const &name, u32 k, ObjType ot) {
  assert_stmt(
      list_length(expr) >= k,
      "Should check for argument list length before calling expect_arg_type");
  Object *arg = list_index(expr, k);
  if (arg->type != ot) {
    error_msg(format("\"{}\" expects {}-th argument to be a \"{}\", got \"{}\"",
                     name, k, obj_type_to_str(ot), obj_type_to_str(arg->type)));
    return false;
  }
  return true;
}

Object *input_builtin(Object *expr) {
  if (!expect_args_check(expr, "input", EA::LEQ, 2)) return nil_obj;
  bool has_prompt = list_length(expr) == 2;
  if (has_prompt) {
    if (!expect_arg_type(expr, "input", 1, ObjType::String)) return nil_obj;
    Object *prompt = list_index(expr, 1);
    auto *prompt_s = prompt->val.s_value;
    std::cout << *prompt_s;
  }
  auto *input = new std::string();
  std::cin >> *input;
  std::cout << '\n';
  Object *res = create_str_obj(input);
  return res;
}

Object *make_hash_table_builtin(Object *expr) {
  // TODO: Process arguments
  return create_hash_table_obj();
}

Object *get_hash_table_builtin(Object *expr) {
  if (!check_builtin_n_params("get-hash", expr, 2)) return nil_obj;
  auto *ht = eval_expr(list_index(expr, 1));
  auto *key = eval_expr(list_index(expr, 2));
  auto *val = hash_table_get(ht, key);
  return val;
}

Object *set_hash_table_builtin(Object *expr) {
  if (!check_builtin_n_params("set-hash", expr, 3)) return nil_obj;
  auto *ht = eval_expr(list_index(expr, 1));
  auto *key = eval_expr(list_index(expr, 2));
  auto *val = eval_expr(list_index(expr, 3));
  hash_table_set(ht, key, val);
  return nil_obj;
}

Object *null_builtin(Object *expr) {
  auto *e = list_index(expr, 1);
  auto *ee = eval_expr(e);
  return is_truthy(ee) ? false_obj : true_obj;
}

static size_t call_stack_size = 0;
const size_t MAX_STACK_SIZE = 256;

Object *call_function(Object *fobj, Object *args_list) {
  if (call_stack_size > MAX_STACK_SIZE) {
    error_msg("Max call stack size reached");
    return nil_obj;
  }

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
  for (size_t arg_idx = starting_arg_idx; arg_idx < arglistl->size();
       ++arg_idx) {
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
      for (auto provided_arg_idx = arg_idx;
           provided_arg_idx < provided_arglistl->size(); ++provided_arg_idx) {
        auto *provided_arg = provided_arglistl->at(provided_arg_idx);
        // user provided a dot argument, which means that a list containing
        // all the rest of variadic arguments must follow
        if (provided_arg == dot_obj) {
          // the dot must be on the pre-last position
          if (provided_arg_idx != provided_arglistl->size() - 2) {
            auto *fn = fun_name(fobj);
            error_msg(format(
                "Error while calling {}: dot notation on the caller side "
                "must be followed by a list argument containing the "
                "variadic expansion list\n",
                fn));
            return nil_obj;
          }
          // expand the rest
          auto *provided_variadic_list =
              eval_expr(provided_arglistl->at(provided_arg_idx + 1));
          if (provided_variadic_list->type != ObjType::List) {
            error_msg(
                "dot operator on caller side should always be "
                "followed by a list argument");
            return nil_obj;
          }
          for (size_t exp_idx = 0;
               exp_idx < list_length(provided_variadic_list); ++exp_idx) {
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
  ++call_stack_size;
  enter_scope_with(locals);
  while (body_expr_idx < body_length) {
    if (last_evaluated != nil_obj) {
      dec_ref(last_evaluated);
    }
    last_evaluated = eval_expr(bodyl->at(body_expr_idx));
    if (last_evaluated != nil_obj) {
      inc_ref(last_evaluated);
    }
    ++body_expr_idx;
  }
  exit_scope();
  --call_stack_size;
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
        for (size_t i = 0; i < items->size(); ++i) {
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
        auto *os = obj_to_string_bare(op);
        error_msg(
            format("\"{}\" (eval: {}) is not callable", s->data(), os->data()));
        delete s;
        delete os;
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

bool load_file(path file_to_read) {
  assert_stmt(IS.running, "");
  auto s = read_whole_file_into_memory(file_to_read.c_str());
  IS.text = s.c_str();
  IS.file_name = file_to_read.c_str();
  IS.line = 1;
  IS.col = 0;
  if (IS.text == nullptr) {
    printf("Couldn't load file at %s, skipping\n", file_to_read.c_str());
    IS.running = false;
    return false;
  }
  IS.text_len = strlen(IS.text);
  IS.text_pos = 0;
  while (IS.text_pos < IS.text_len) {
    auto *e = read_expr();
    eval_expr(e);
  }
  return true;
}

void gc_task() {
  GC.log_file =
      new std::ofstream(GC_LOG_FILE, std::ios_base::app | std::ios_base::ate);
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using std::chrono::high_resolution_clock;
  using std::chrono::milliseconds;
  // auto &gc_out = GC.log_file;
  auto &gc_out = *GC.log_file;
  gc_out << "Initializing GC..." << std::endl;
  while (IS.running) {
    gc_out << "Cleaning up... ";
    auto start_time = high_resolution_clock::now();
    u32 objects_total = 0;
    u32 objects_deleted = 0;
    // do gc
    for (auto it = IS.objects_pool.begin(); it != IS.objects_pool.end(); ++it) {
      auto *curr = *it;
      const bool persistent = curr->flags & OF_PERSISTENT;
      if (curr->ref == 0 && !persistent) {
        auto prev_it = it;
        ++it;
        IS.objects_pool.erase(prev_it);
        delete_obj(curr);
      } else {
        objects_total += 1;
      }
    }
    auto end_time = high_resolution_clock::now();
    duration<double, std::milli> ms_double = end_time - start_time;
    auto running_time = ms_double.count();
    gc_out << format("deleted {} objects, {} total. Took {} ms",
                     objects_deleted, objects_total, running_time);
    gc_out << std::endl;
    std::this_thread::sleep_for(GC_INTERVAL);
  }
}

void init_gc() { GC.thread = new std::thread(gc_task); }

void init_interp() {
  // Initialize global symbol table
  IS.symtable = new SymTable();
  IS.symtable->prev = nullptr;
  nil_obj = create_nil_obj();
  true_obj = create_bool_obj(true);
  false_obj = create_bool_obj(false);
  dot_obj = create_final_sym_obj(".");
  else_obj = create_final_sym_obj("else");
  // initialize symtable with builtins
  set_symbol("nil", nil_obj);
  set_symbol("true", true_obj);
  set_symbol("false", false_obj);
  set_symbol("else", else_obj);
  create_builtin_function_and_save("null?", (null_builtin));
  create_builtin_function_and_save("+", (add_objects));
  create_builtin_function_and_save("-", (sub_objects));
  create_builtin_function_and_save("/", (div_objects_builtin));
  create_builtin_function_and_save("remainder", (div_objects_rem));
  create_builtin_function_and_save("*", (mul_objects_builtin));
  create_builtin_function_and_save("**", (pow_objects_builtin));
  create_builtin_function_and_save("=", (equal_builtin));
  create_builtin_function_and_save(">", (gt_builtin));
  create_builtin_function_and_save("<", (lt_builtin));
  create_builtin_function_and_save("not", (not_builtin));
  create_builtin_function_and_save("setq", (setq_builtin));
  create_builtin_function_and_save("print", (print_builtin));
  create_builtin_function_and_save("begin", (begin_builtin));
  create_builtin_function_and_save("defun", (defun_builtin));
  create_builtin_function_and_save("lambda", (lambda_builtin));
  create_builtin_function_and_save("eval", (eval_builtin));
  create_builtin_function_and_save("if", (if_builtin));
  create_builtin_function_and_save("car", (car_builtin));
  create_builtin_function_and_save("cdr", (cdr_builtin));
  create_builtin_function_and_save("cadr", (cadr_builtin));
  create_builtin_function_and_save("cond", (cond_builtin));
  create_builtin_function_and_save("let", (let_builtin));
  create_builtin_function_and_save("cons", (cons_builtin));
  create_builtin_function_and_save("memtotal", (memtotal_builtin));
  create_builtin_function_and_save("timeit", (timeit_builtin));
  create_builtin_function_and_save("sleep", (sleep_builtin));
  create_builtin_function_and_save("make-hash-table",
                                   (make_hash_table_builtin));
  create_builtin_function_and_save("get-hash", (get_hash_table_builtin));
  create_builtin_function_and_save("set-hash", (set_hash_table_builtin));
  create_builtin_function_and_save("input", (input_builtin));
  // setup gc
  IS.running = true;
  init_gc();
  // Load the standard library
  path STDLIB_PATH = "./stdlib";
  load_file(STDLIB_PATH / path("basic.lisp"));
}

void run_interp() {
  assert_stmt(IS.running, "");
  // std::string prev_input = "";
  std::string input;
  static std::string prompt = ">> ";
  IS.file_name = "interp";
  IS.line = 1;
  IS.col = 0;

  // auto prev_history_item = [&]() -> void {
  //   // std::cout << "prev history item" << std::endl;
  //   if (prev_input != "") {
  //     input = prev_input;
  //     std::cout << input;
  //   }
  // };
  // const std::string up = "\033[A";
  // const std::string down = "\033[B";
  // const std::string left = "\033[D";
  // const std::string right = "\033[C";
  // const std::unordered_map<std::string, std::function<void()>>
  //     escape_sequences = {{up, prev_history_item}};

  // std::string escape_seq_s = "";
  // bool reading_escape_seq = false;
  char c;
  while (IS.running) {
    std::cout << prompt;
    while (std::cin.get(c)) {
      // if (reading_escape_seq) {
      //   escape_seq_s += c;
      //   // std::cout << "reading-escape-seq: " << escape_seq_s << std::endl;
      //   if (escape_sequences.find(escape_seq_s) != escape_sequences.end()) {
      //     auto handler = escape_sequences.at(escape_seq_s);
      //     handler();
      //     escape_seq_s = "";
      //   }
      // } else if (c == '\033') {
      //   escape_seq_s += c;
      //   reading_escape_seq = true;
      // } else
      if (c == '\n') {
        break;
      } else {
        input += c;
      }
    }
    // if (reading_escape_seq) {
    //   reading_escape_seq = false;
    //   continue;
    // }
    // prev_input = input;
    if (input == ".exit") {
      IS.running = false;
      continue;
    }
    IS.text = input.data();
    IS.text_pos = 0;
    IS.text_len = input.size();
    auto *e = read_expr();
    if (e != nullptr) {
      auto *res = eval_expr(e);
      auto *str_repr = obj_to_string_bare(res);
      std::cout << str_repr->data() << '\n';
      delete str_repr;
    }
    input = "";
  }
}
