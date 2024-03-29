#include "Common.h"
#include "my_p99_common.h"
#include <getopt.h>

#include "ast.h"
#include "parser.tab.h"

#include "lexer.h"

#define LPUTS(stream, str) fwrite(("" str ""), 1, (sizeof(str) - 1), (stream))
#define INDENT_WIDTH 2

static int parse_data(ast_data *data, const char *out_fname);
static void print_data(ast_node *node, bstring *out);

#define DO_IT(s, o)                                  \
        do {                                         \
                ast_data *data = ast_data_create(s); \
                parse_data(data, (o));               \
                talloc_free(data);                   \
        } while (0)

/*======================================================================================*/

void
recompile_main(const char *fname, const char *out_fname)
{
        if (fname)
                DO_IT(fname, out_fname);
        else
                DO_IT(stdin, out_fname);
}

static inline void
pspaces(bstring *out, unsigned num)
{
        if (num-- == 0)
                return;
        num *= INDENT_WIDTH;
        while (num--)
                b_catchar(out, ' ');
}

static int
parse_data(ast_data *data, const char *out_fname)
{
        FILE *out_fp = (!out_fname || strcmp(out_fname, "-") == 0)
                           ? stdout
                           : safe_fopen(out_fname, "wb");
        yyscan_t scanner;
        int      ret;

        yylex_init(&scanner);
        yyset_extra(data, scanner);
        yyset_in(data->fp_wrap->fp, scanner);
        data->column = 0;
        ret          = yyparse(scanner, data);
        yylex_destroy(scanner);

        if (ret == 0)
                (void)0;
        
        bstring *out = b_create(8192);
        print_data(data->top, out);
        b_fwrite(out_fp, out);
        b_destroy(out);

        if (out_fp != stdout)
                fclose(out_fp);
        return ret;
}


static void
print_data(ast_node *node, bstring *out)
{
        if (node->type != NODE_BLOCK)
                pspaces(out, node->depth);

        switch (node->type) {
        case NODE_BLANK_LINE:
                b_catchar(out, '\n');
                return; /* Return early */
        case NODE_BLOCK:
                GENLIST_FOREACH (node->block.list, ast_node *, sub)
                        print_data(sub, out);
                pspaces(out, node->depth);
                if (node->block.name)
                        b_sprintfa(out, "</%s", node->block.name);
                break;
        case NODE_COMMENT:
                b_sprintfa(out, "<!--%s-->\n", node->comment);
                return; /* Return early */
        case NODE_ST_UNIMPL:
                b_sprintfa(out, "<%s", node->unimpl.id);
                GENLIST_FOREACH (node->unimpl.list, ast_atom *, atom)
                        b_sprintfa(out, " %s=%s", atom->unimpl.id, atom->unimpl.text);
                break;
        case NODE_ST_ASSIGN:
                b_sprintfa(out, "<set_value name=\"%s\"", node->assignment.var);
                switch (node->assignment.type) {
                case ASSIGNMENT_NORMAL:
                        if (node->assignment.expr)
                                b_sprintfa(out, " exact=\"%s\"", node->assignment.expr);
                        break;
                case ASSIGNMENT_SPECIAL:
                        b_sprintfa(out, " %s", node->assignment.expr);
                        break;
                case ASSIGNMENT_ADD:
                        b_sprintfa(out, " operation=\"add\"");
                        break;
                default:;
                }
                break;
        case NODE_ST_IF:
                b_sprintfa(out, "<do_if value=\"%s\"", node->condition);
                break;
        case NODE_ST_ELSIF:
                b_sprintfa(out, "<do_elseif value=\"%s\"", node->condition);
                break;
        case NODE_ST_ELSE:
                b_sprintfa(out, "<do_else");
                break;
        case NODE_ST_WHILE:
                b_sprintfa(out, "<do_while value=\"%s\"", node->condition);
                break;
        case NODE_ST_FOR:
                b_sprintfa(out, "<do_all exact=\"%s\" counter=\"%s\"", node->forstmt.var, node->forstmt.ident);
                if (node->forstmt.reversed)
                        b_sprintfa(out, " reverse=\"true\"");
                break;
        case NODE_ST_DEBUG_TEXT:
                b_sprintfa(out, "<debug_text text=\"%s\"", node->debug.text);
                if (node->debug.filter)
                        b_sprintfa(out, " filter=\"%s\"", node->debug.filter);
                break;
        case NODE_ST_RETURN:
        case NODE_ST_BREAK:
                b_sprintfa(out, "<%s", node->keyword);
                break;
        case NODE_ST_UNDEF:
                b_sprintfa(out, "<remove_value name=\"%s\"", node->string);
                break;
        default:
                eprintf("Unknown node: %d\n", node->type);
                break;
        }

        if (node->chance)
                b_sprintfa(out, " chance=\"%s\"", node->chance);

        if (node->line_comment)
                b_sprintfa(out, " comment=\"%s\"", node->line_comment);

        if (node->depth > 0) {
                if (node->block_parent || node->type == NODE_BLOCK)
                        b_sprintfa(out, ">\n");
                else
                        b_sprintfa(out, "/>\n");
        }
}
