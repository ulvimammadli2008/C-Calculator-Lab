// Ulvi Mammadli
// Student ID: 251ADB078
// Repository: https://github.com/ulvimammadli2008/C-Calculator-Lab
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: OpenAI ChatGPT (this conversation; code review and comment assistance).
// Conversation history: https://chatgpt.com/share/6ae0fd1a-bc00-83ed-8ea5-d0f8ff36cccf


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

// The type field determines which value field should be used.
typedef enum { VALUE_INT, VALUE_FLOAT } ValueType;

typedef struct {
  ValueType type;
  long long i;
  double f;
} Value;

// Stores the current expression and parser position.
// line_start allows error positions to be reported relative to the whole input file.
// has_error prevents the parser from overwriting the first detected error.
typedef struct {
  const char* text;
  size_t length;
  size_t index;
  size_t line_start;
  size_t error_pos;
  int has_error;
} Parser;

// Advances the parser past whitespace between tokens
static void skip_spaces(Parser* p) {
  while (p->index < p->length && (p->text[p->index] == ' ' || p->text[p->index] == '\t' || p->text[p->index] == '\r')) {
    p->index++;
  }
}

// Records the first parsing error and its one-based position. Later errors do not everwrite the original error.
static void set_error(Parser* p, size_t pos) {
  if (!p->has_error) {
    p->has_error = 1;
    p->error_pos = pos;
  }
}

// Parser and integer or decimal number and checks for invalid or out-of-range values 
static Value parse_number(Parser* p) {
  Value v = {VALUE_INT, 0, 0.0};
  size_t start;
  size_t digits_before = 0;
  size_t digits_after = 0;

  skip_spaces(p);
  start = p->index;

  while (p->index < p->length && isdigit((unsigned char)p->text[p->index])) {
    p->index++;
    digits_before++;
  }

  if (p->index < p->length && p->text[p->index] == '.') {
    size_t dot = p->index;
    p->index++;

    while (p->index < p->length && isdigit((unsigned char)p->text[p->index])) {
      p->index++;
      digits_after++;
    }

    if (digits_before == 0 || digits_after == 0) {
      set_error(p, p->line_start + dot + 1);
      return v;
    }

    {
      char* end;
      char* copy = malloc(p->index - start + 1);
      if (copy == NULL) {
        set_error(p, p->line_start + start + 1);
        return v;
      }

      memcpy(copy, p->text + start, p->index - start);
      copy[p->index - start] = '\0';
      errno = 0;
      v.f = strtod(copy, &end);

      if (errno == ERANGE || !isfinite(v.f) || end == copy || *end != '\0') {
        set_error(p, p->line_start + start + 1);
      } else {
        v.type = VALUE_FLOAT;
      }
      free(copy);
    }
    return v;
  }

  if (digits_before == 0) {
    set_error(p, p->line_start + start + 1);
    return v;
  }

  {
    char* end;
    char* copy = malloc(p->index - start + 1);
    if (copy == NULL) {
      set_error(p, p->line_start + start + 1);
      return v;
    }

    memcpy(copy, p->text + start, p->index - start);
    copy[p->index - start] = '\0';
    errno = 0;
    v.i = strtoll(copy, &end, 10);

    if (errno == ERANGE || end == copy || *end != '\0') {
      set_error(p, p->line_start + start + 1);
    }
    free(copy);
  }

  return v;
}

static Value parse_expression(Parser* p);
static Value parse_power(Parser* p);

// Creates a Value containing an integer.
static Value make_int(long long n) {
  Value v = {VALUE_INT, n, 0.0};
  return v;
}

// Creates a Value containing a floating-point number.
static Value make_float(double n) {
  Value v = {VALUE_FLOAT, 0, n};
  return v;
}

// Converts a Value to double for calculations involving floating-point numbers.
static double as_double(Value v) {
  return v.type == VALUE_INT ? (double)v.i : v.f;
}

// Returns 1 if the addition fits in a long long; otherwise returns 0
static int safe_add(long long a, long long b, long long* result){
  if ((b > 0 && a > LLONG_MAX - b) || 
      (b < 0 && a < LLONG_MIN - b)) {
    return 0;
  }

  *result = a + b;
  return 1;
}

// Returns 1 if the subtraction fits in a long long; otherwise returns 0
static int safe_subtract(long long a, long long b, long long* result){
  if ((b < 0 && a > LLONG_MAX + b) ||
      (b > 0 && a < LLONG_MIN + b)){
        return 0;
  }

  *result = a - b;
  return 1;
}

// Checks multiplication for signed intefer overflow before calculating the result
static int safe_multiply(long long a, long long b, long long* result){
  if (a == 0 || b == 0){
    *result = 0;
    return 1;
  }

  if ((a == -1 && b == LLONG_MIN) || 
      (b == -1 && a == LLONG_MIN)){
    return 0;
    }

  if (a > 0){
    if ((b > 0 && a > LLONG_MAX / b) ||
        (b < 0 && b < LLONG_MIN / a)) {
      return 0;
    }
  } else {
    if ((b > 0 && a < LLONG_MIN / b) ||
        (b < 0 && a < LLONG_MAX / b)) {
      return 0;
    }
  }

  *result = a * b;
  return 1;
}

// Parses numbers and parenthesized expressions
static Value parse_primary(Parser* p) {
  Value v = make_int(0);

  skip_spaces(p);

  if (p->index >= p->length) {
    set_error(p, p->line_start + p->length + 1);
    return v;
  }

  if (p->text[p->index] == '(') {
    p->index++;
    v = parse_expression(p);
    skip_spaces(p);

    if (p->index >= p->length) {
      set_error(p, p->line_start + p->length + 1);
    } else if (p->text[p->index] == ')') {
      p->index++;
    } else {
      set_error(p, p->line_start + p->index + 1);
    }
    return v;
  }

  if (isdigit((unsigned char)p->text[p->index]) || p->text[p->index] == '.') {
    return parse_number(p);
  }

  set_error(p, p->line_start + p->index + 1);
  return v;
}

// Parser exppnentiation recursiverly making chained powers right-associative 
static Value parse_power(Parser* p) {
  Value left = parse_primary(p);
  size_t op_pos;

  skip_spaces(p);

  if (p->index + 1 < p->length && p->text[p->index] == '*' &&
      p->text[p->index + 1] == '*') {
    Value right;
    double result;

    op_pos = p->index;
    p->index += 2;
    skip_spaces(p);
    size_t right_pos = p->index;

    if (p->index >= p->length || p->text[p->index] == ')' ||
        p->text[p->index] == '+' || p->text[p->index] == '-' ||
        p->text[p->index] == '*' || p->text[p->index] == '/') {
      set_error(p, p->line_start + p->index + 1);
      return left;
    }

    right = parse_power(p);

    if (left.type == VALUE_INT && right.type == VALUE_INT) {
      if (right.i < 0) {
        set_error(p, p->line_start + right_pos + 1);
        return left;
      }

      {
        long long result_int = 1;
        long long base = left.i;
        unsigned long long exponent = (unsigned long long)right.i;

        while (exponent > 0) {
          if (exponent & 1ULL) {
            long long temp;

            if (!safe_multiply(result_int, base, &temp)) {
              set_error(p, p->line_start + op_pos + 1);
              return left;
            }

            result_int = temp;
          }

          exponent >>= 1;

          if (exponent > 0) {
            long long temp;

            if(!safe_multiply(base, base, &temp)) {
              set_error(p, p->line_start + op_pos + 1);
              return left;
            }

            base = temp;
          }
        }
        return make_int(result_int);
      }
    }

    result = pow(as_double(left), as_double(right));
    if (!isfinite(result)) {
      set_error(p, p->line_start + op_pos + 1);
      return left;
    }
    return make_float(result);
  }

  return left;
}

// Parses multiplication and division after exponentiation
static Value parse_multiplication(Parser* p){
  Value left = parse_power(p);
  
  while(!p->has_error) {
    size_t op_pos;
    char op;
    Value right;

    skip_spaces(p);

    if(p->index >= p->length || (p->text[p->index] != '*' && p->text[p->index] != '/')){
      break;
    }

    if (p->text[p->index] == '*' && p->index + 1 < p->length && p->text[p->index + 1] == '*'){
      break;
    }

    op_pos = p->index;
    op = p->text[p->index++];

    skip_spaces(p);
    size_t right_pos = p->line_start + p->index;

    if(p->index >= p->length || p->text[p->index] == ')' || p->text[p->index] == '+' || p->text[p->index] == '-' || p->text[p->index] == '*' || p->text[p->index] == '/'){
      set_error(p, p->line_start + p->index +1);
      break;
    }

    right = parse_power(p);

    if(p->has_error){
      break;
    }

    if (op == '/') {
      if ((right.type == VALUE_INT && right.i == 0) || (right.type == VALUE_FLOAT && right.f == 0.0)){
        set_error(p, right_pos + 1);
        break;
      }

      if (left.type == VALUE_INT && right.type == VALUE_INT){
        if(left.i == LLONG_MIN && right.i == -1){
          set_error(p, p->line_start + op_pos + 1);
          break;
        }

        left.i /= right.i;
      } else {
        left = make_float(as_double(left) / as_double(right));

        if (!isfinite(left.f)){
          set_error(p, p->line_start + op_pos + 1);
          break;
        }
      }
    } else if (left.type == VALUE_INT && right.type == VALUE_INT){
      long long result;

      if (!safe_multiply(left.i, right.i, &result)){
        set_error(p, p->line_start + op_pos + 1);
        break;
      }

      left.i = result;
    } else {
      left = make_float(as_double(left) * as_double(right));

      if (!isfinite(left.f)){
        set_error(p, p->line_start + op_pos + 1);
        break;
      }
    }
  }

  return left;
}

// Parses addition and subsctraction after multiplication and division
static Value parse_expression(Parser* p){
  Value left = parse_multiplication(p);

  while (!p->has_error){
    size_t op_pos;
    char op;
    Value right;

    skip_spaces(p);

    if (p->index >= p->length || (p->text[p->index] != '+' && p->text[p->index] != '-')){
      break;
    }

    op_pos = p->index;
    op = p->text[p->index++];

    skip_spaces(p);

    if (p->index >= p->length || p->text[p->index] == ')' || p->text[p->index] == '+' || p->text[p->index] == '-' || p->text[p->index] == '*' || p->text[p->index] == '/'){
      set_error(p, p->line_start + p->index + 1);
      break;
    }

    right = parse_multiplication(p);

    if (p->has_error){
      break;
    }

    if (left.type == VALUE_INT && right.type == VALUE_INT){
      long long result;
      
      if (op == '+'){
        if (!safe_add(left.i, right.i, &result)){
          set_error(p, p->line_start + op_pos + 1);
          break;
        }
      } else {
        if (!safe_subtract(left.i, right.i, &result)){
          set_error(p, p->line_start + op_pos + 1);
          break;
        }
      }

      left.i = result;
    } else {
      double result = op == '+'
        ? as_double(left) + as_double(right)
        : as_double(left) - as_double(right);

      if (!isfinite(result)){
        set_error(p, p->line_start + op_pos + 1);
        break;
      }

      left = make_float(result);
    }
  }

  return left;
}

// Parses a complete expression and reports any unconsumed invalid characters
static Value evaluate_expression(Parser* p){
  Value result = parse_expression(p);

  skip_spaces(p);

  if (!p->has_error && p->index < p->length){
    set_error(p, p->line_start + p->index + 1);
  }

  return result;
}

// Writes the calculated value using the appropriate numeric format
static void print_value(FILE* out, Value v){
  if (v.type == VALUE_INT) {
    fprintf(out, "%lld\n", v.i);
  } else {
    fprintf(out, "%.15g\n", v.f);
  }
}

// Skips blank lines and comments, evaluates the expression, and records errors.
static void process_line(const char* text, size_t length, size_t line_start, FILE* out, size_t* error_pos, int* has_error){
  size_t first = 0;

  while (first < length && (text[first] == ' ' || text[first] == '\t')){
    first++;
  }

  if (first == length || text[first] == '#'){
    return;
  }

  Parser p = {
    text, length, 0, line_start, 0, 0
  };

  Value result = evaluate_expression(&p);

  if (p.has_error) {
    if (!*has_error) {
        *error_pos = p.error_pos;
    }
    *has_error = 1;
    return;
  }

  print_value(out, result);
}

// Processes the input while buffering results so an error can replace normal output.
static int process_input(FILE* in, FILE* out, size_t* error_pos){
  // Store results temporarily so that, if any expression is invalid,
  // the output file contains the error message instead of partial results.
  FILE* temp = tmpfile();

  if (temp == NULL){
    return -1;
  }

  char* buffer = NULL;
  size_t capacity = 0;
  size_t length = 0;
  size_t file_ops = 0;
  int ch;
  int has_error = 0;

  while ((ch = fgetc(in)) != EOF){
    if (length + 1 >= capacity) {
      size_t new_capacity = capacity == 0 ? 256 : capacity * 2;
      char* new_buffer = realloc(buffer, new_capacity);

      if (new_buffer == NULL) {
        free(buffer);
        fclose(temp);
        return -1;
      }

      buffer = new_buffer;
      capacity = new_capacity;
    }

    if (ch == '\n') {
      process_line(buffer, length, file_ops, temp, error_pos, &has_error);
      file_ops += length + 1;
      length = 0;
    } else {
      buffer[length++] = (char)ch;
    }
  }

  if (ferror(in)){
    free(buffer);
    fclose(temp);
    return -1;
  }

  if (length > 0) {
    process_line(buffer, length, file_ops, temp, error_pos, &has_error);
  }

  free(buffer);


 if (has_error){
   if (fprintf(out, "ERROR:%zu\n", *error_pos) < 0 || ferror(out)) {
    fclose(temp);
    return -1;
  }

  fclose(temp);
  return 0;

 }


  rewind(temp);

  while ((ch = fgetc(temp)) != EOF){
    if (fputc(ch, out) == EOF) {
      fclose(temp);
      return -1;
    }
  }

  if(ferror(temp) || ferror(out)){
    fclose(temp);
    return -1;
  }

  fclose(temp);
  return 0;
}

// Stores the input file or directory and the optional output directory.
typedef struct {
  char* input_file;
  char* input_dir;
  char* output_dir;
} Options;

// Parses and validates the command-line arguments.
static int parse_options(int argc, char* argv[], Options* options){
  options->input_file = NULL;
  options->input_dir = NULL;
  options->output_dir = NULL;

  for (int i = 1; i < argc; i++){
    if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0){
      if (i + 1 >= argc || options->input_dir != NULL || options->input_file != NULL){
        return 0;
      }

      options->input_dir = argv[++i];
    } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0){
      if (i + 1 >= argc || options->output_dir != NULL){
        return 0;
      }

      options->output_dir = argv[++i];
    } else if (argv[i][0] == '-') {
      return 0;
    } else {
      if (options->input_file != NULL || options->input_dir != NULL) {
        return 0;
      }

      options->input_file = argv[i];
    }
  }

  if (options->input_file == NULL && options->input_dir == NULL) {
    return 0;
  }

  return 1;
}

// Checks whether the output directory exists or creates it.
static int make_directory(const char* path){
  struct stat info;

  if (stat(path, &info) == 0) {
    return S_ISDIR(info.st_mode);
  }

  if (errno != ENOENT) {
    return 0;
  }

  return mkdir(path, 0777) == 0;
}

// Combines a directory path and filename into a newly allocated path.
static char* join_path(const char* directory, const char* name){
  size_t dir_len = strlen(directory);
  size_t name_len = strlen(name);
  int needs_slash = dir_len > 0 && directory[dir_len - 1] != '/';
  char* result = malloc(dir_len + (size_t)needs_slash + name_len + 1);

  if (result == NULL){
    return NULL;
  }

  memcpy(result, directory, dir_len);

  if (needs_slash) {
    result[dir_len] = '/';
  }

  memcpy(result + dir_len + (size_t)needs_slash, name, name_len + 1);
  return result;
}

// Creates an output filename using the input name and student details.
static char* make_output_filename(const char* input_path) {
  const char* base = strrchr(input_path, '/');
  base = base == NULL ? input_path : base + 1;

  size_t base_len = strlen(base);
  const char* dot = strrchr(base, '.');

  if (dot != NULL && dot != base){
    base_len = (size_t)(dot - base);
  }

  const char* first_name = "Ulvi";
  const char* last_name = "Mammadli";
  const char* student_id = "251ADB078";
  
  size_t needed = base_len + strlen(first_name) + strlen(last_name) + strlen(student_id) + 8;

  char* filename = malloc(needed);
  if(filename == NULL){
    return NULL;
  }

  snprintf(filename, needed, "%.*s_%s_%s_%s.txt", (int)base_len, base, first_name, last_name, student_id);

  return filename;
}

// Opens an input file, processes it, writes the output, and closes both files.
static int process_file(const char* input_path, const char* output_dir) {
  FILE* in = fopen(input_path, "rb");

  if (in == NULL) {
    fprintf(stderr, "Cannot open input file: %s\n", input_path);
    return 1;
  }

  char* filename = make_output_filename(input_path);
  if (filename == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(in);
    return 1;
  }

  char* output_path = join_path(output_dir, filename);
  free(filename);

  if (output_path == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(in);
    return 1;
  }

  FILE* out = fopen(output_path, "wb");
  if (out == NULL) {
    fprintf(stderr, "Cannot create output file: %s\n", output_path);
    free(output_path);
    fclose(in);
    return 1;
  }

  size_t error_pos = 0;
  int result = process_input(in, out, &error_pos);

  int close_in = fclose(in);
  int close_out = fclose(out);

  if(close_in != 0 || close_out != 0) {
    fprintf(stderr, "Error closing a file\n");
    result = -1;
  }

  if (result < 0) {
    fprintf(stderr, "Error processing file: %s\n", input_path);
    result = 1;
  }

  free(output_path);
  return result;
}

// Processes regular .txt files found in the specified directory.
static int process_directory(const char* input_dir, const char* output_dir) {
  DIR* dir = opendir(input_dir);

  if (dir == NULL) {
    fprintf(stderr, "Cannot open input directory: %s\n", input_dir);
    return 1;
  }

  struct dirent* entry;
  int failed = 0;

  while ((entry = readdir(dir)) != NULL) {
    size_t name_len = strlen(entry->d_name);

    if (name_len < 4 || strcmp(entry->d_name + name_len - 4, ".txt") != 0){
      continue;
    }

    char* input_path = join_path(input_dir, entry->d_name);
    if(input_path == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      failed = 1;
      break;
    }

    struct stat info;
    if(stat(input_path, &info) == 0 && S_ISREG(info.st_mode)) {
      if (process_file(input_path, output_dir) != 0) {
        failed = 1;
      }
    }

    free(input_path);
  }

  closedir(dir);
  return failed;
}

// Validates arguments, selects the output directory, and starts processing.
int main(int argc, char* argv[]) {
  Options options;

  if (!parse_options(argc, argv, &options)) {
    fprintf(stderr, "Usage: %s [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n", argv[0]);
    return 1;
  }

  char* default_output_dir = NULL;
  const char* output_dir = options.output_dir;

  if (options.input_dir != NULL) {
    if (output_dir == NULL) {
      size_t needed = strlen(options.input_dir) + 32;
      default_output_dir = malloc(needed);

      if (default_output_dir == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
      }

      snprintf(default_output_dir, needed, "%s_Ulvi_251ADB078", options.input_dir);
      output_dir = default_output_dir;
    }
  } else {
    if (output_dir == NULL) {
      default_output_dir = make_output_filename(options.input_file);

      if (default_output_dir == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
      }

      char* dot = strrchr(default_output_dir, '.');
      if (dot != NULL) {
        *dot = '\0';
      }

      output_dir = default_output_dir;
    }
  }

  if (!make_directory(output_dir)) {
    fprintf(stderr, "Cannot create output directory: %s\n", output_dir);
    free(default_output_dir);
    return 1;
  }

  int result;

  if (options.input_dir != NULL) {
    result = process_directory(options.input_dir, output_dir);
  } else {
    result = process_file(options.input_file, output_dir);
  }

  free(default_output_dir);
  return result;
}