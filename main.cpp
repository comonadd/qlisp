#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>

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
  return isalpha(ch) || ch == '+' || ch == '-';
}

enum class ObjType { List, Symbol, String, Number, Nil };

int OF_BUILTIN = 0x1;

struct Object;

using Builtin = Object *(*)(Object *);

struct Object {
  ObjType type;
  int flags = 0;
  union {
    int i_value;
    std::string *s_value;
    std::vector<Object *> *l_value;
    Builtin builtin_handler;
  } val;
};

static Object *nil_obj;
std::unordered_map<std::string, Object *> symtable;

Object *new_object(ObjType type, int flags = 0) {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = type;
  res->flags = flags;
  return res;
}

Object *create_nil_obj() { return new_object(ObjType::Nil); }

Object *create_str_obj(std::string *s) {
  auto *res = new_object(ObjType::String);
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

Object *create_builtin_obj(Builtin handler) {
  Object *res = new_object(ObjType::Symbol, OF_BUILTIN);
  res->val.builtin_handler = handler;
  return res;
}

Object *read_str() {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = ObjType::String;
  res->val.s_value = new std::string("");
  consume_char('"');
  char ch = get_char();
  while (text_pos < text_len && ch != '"') {
    res->val.s_value->push_back(ch);
    ch = next_char();
  }
  consume_char('"');
  return res;
}

Object *read_sym() {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = ObjType::Symbol;
  res->val.s_value = new std::string("");
  char ch = get_char();
  while (text_pos < text_len && can_be_a_part_of_symbol(ch)) {
    res->val.s_value->push_back(ch);
    ch = next_char();
  }
  return res;
}

Object *create_num_obj(int v) {
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = ObjType::Number;
  res->val.i_value = v;
  return res;
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
  Object *res = (Object *)malloc(sizeof(*res));
  res->type = ObjType::List;
  res->val.l_value = new std::vector<Object *>();
  consume_char('(');
  while (get_char() != ')') {
    if (text_pos >= text_len) {
      printf("EOF\n");
      exit(1);
      return nullptr;
    }
    auto *e = read_expr();
    res->val.l_value->push_back(e);
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
  symtable[*symname->val.s_value] = symvalue;
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

Object *eval_expr(Object *expr) {
  switch (expr->type) {
  case ObjType::Number:
    return expr;
  case ObjType::String:
    return expr;
  case ObjType::Symbol: {
    // Look up value of the symbol in the symbol table
    auto *syms = expr->val.s_value;
    bool present_in_symtable = symtable.find(*syms) != symtable.end();
    if (!present_in_symtable) {
      printf("Symbol not found %s\n", syms->data());
      return nil_obj;
    }
    return symtable[*syms];
  } break;
  case ObjType::List: {
    auto *l = expr->val.l_value;
    int elems_len = l->size();
    if (elems_len == 0)
      return expr;
    auto *op = l->at(0);
    int args_len = elems_len - 1;
    bool present_in_symtable =
        symtable.find(*op->val.s_value) != symtable.end();
    auto *op_s = op->val.s_value;
    // printf("reading Symbol %s\n", op_s->data());
    if (!present_in_symtable) {
      printf("Symbol not found %s\n", op_s->data());
      return nil_obj;
    }
    auto *callable = symtable[*op->val.s_value];
    bool is_builtin = callable->flags & OF_BUILTIN;
    if (is_builtin) {
      auto *bhandler = callable->val.builtin_handler;
      return bhandler(expr);
    }
    return nil_obj;
  }
  case ObjType::Nil: {
    return expr;
  } break;
  default: {
    printf("Unknown expression\n");
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
  nil_obj = create_nil_obj();
  // initialize symtable with builtins
  symtable["+"] = create_builtin_obj(add_objects);
  symtable["-"] = create_builtin_obj(sub_objects);
  symtable["setq"] = create_builtin_obj(setq_builtin);
  symtable["print"] = create_builtin_obj(print_builtin);
  // symtable["*"] = create_builtin_obj(mul_objects);
  // symtable["/"] = create_builtin_obj(div_objects);
}

int main(int argc, char **argv) {
  text = read_whole_file_into_memory("./examples/basic.lisp");
  text_len = strlen(text);

  init_interp();

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
  return 0;
}
