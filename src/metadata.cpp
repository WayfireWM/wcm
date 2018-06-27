/*
 * Copyright © 2018 Scott Moreau
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "wcm.h"

static void
get_plugin_data(Plugin *p, Option *opt, xmlDoc *doc, xmlNode * a_node)
{
        Option *o = opt;
        xmlNode *cur_node = NULL;
        xmlChar *prop;

        for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
                if (cur_node->type == XML_ELEMENT_NODE) {
                        if (std::string((char *) cur_node->name) == "plugin") {
                                prop = xmlGetProp(cur_node, (xmlChar *) "name");
                                if (prop)
                                        p->name = strdup((char *) prop);
                                free(prop);
                        } else if (std::string((char *) cur_node->name) == "_short") {
                                if (!o)
                                        p->disp_name = strdup((char *) cur_node->children->content);
                                else
                                        o->disp_name = strdup((char *) cur_node->children->content);
                        } else if (std::string((char *) cur_node->name) == "category") {
                                if (!cur_node->children)
                                        continue;
                                free(p->category);
                                p->category = strdup((char *) cur_node->children->content);
                        } else if (std::string((char *) cur_node->name) == "option") {
                                o = new Option();
                                o->plugin = p;
                                prop = xmlGetProp(cur_node, (xmlChar *) "name");
                                if (prop)
                                        o->name = strdup((char *) prop);
                                free(prop);
                                prop = xmlGetProp(cur_node, (xmlChar *) "type");
                                if (prop) {
                                        if (std::string((char *) prop) == "int") {
                                                o->type = OPTION_TYPE_INT;
                                                o->data.min = -DBL_MAX;
                                                o->data.max = DBL_MAX;
                                        } else if (std::string((char *) prop) == "double") {
                                                o->type = OPTION_TYPE_DOUBLE;
                                                o->data.min = -DBL_MAX;
                                                o->data.max = DBL_MAX;
                                                o->data.precision = 0.1;
                                        } else if (std::string((char *) prop) == "bool") {
                                                o->type = OPTION_TYPE_BOOL;
                                        } else if (std::string((char *) prop) == "string") {
                                                o->type = OPTION_TYPE_STRING;
                                                o->default_value.s = strdup("");
                                        } else if (std::string((char *) prop) == "button") {
                                                o->type = OPTION_TYPE_BUTTON;
                                                o->default_value.s = strdup("");
                                        } else if (std::string((char *) prop) == "color") {
                                                o->type = OPTION_TYPE_COLOR;
                                                o->default_value.s = strdup("");
                                        } else if (std::string((char *) prop) == "key") {
                                                o->type = OPTION_TYPE_KEY;
                                                o->default_value.s = strdup("");
                                        } else {
                                                printf("WARN: [%s] unknown option type\n", p->name);
                                                o->type = (option_type) -1;
                                        }
                                } else {
                                        printf("WARN: [%s] no option type found\n", p->name);
                                        o->type = (option_type) -1;
                                }
                                free(prop);
                                p->options.push_back(o);
                        } else if (std::string((char *) cur_node->name) == "default") {
                                if (!cur_node->children)
                                        continue;
                                switch (o->type) {
                                        case OPTION_TYPE_INT:
                                                o->default_value.i = atoi((char *) cur_node->children->content);
                                                break;
                                        case OPTION_TYPE_BOOL:
                                                if (std::string((char *) cur_node->children->content) == "true")
                                                        o->default_value.i = 1;
                                                else if (std::string((char *) cur_node->children->content) == "false")
                                                        o->default_value.i = 0;
                                                else
                                                        o->default_value.i = atoi((char *) cur_node->children->content);
                                                if (o->default_value.i < 0 && o->default_value.i > 1)
                                                        printf("WARN: [%s] unknown bool option default\n", p->name);
                                                break;
                                        case OPTION_TYPE_STRING:
                                        case OPTION_TYPE_BUTTON:
                                        case OPTION_TYPE_COLOR:
                                        case OPTION_TYPE_KEY:
                                                free(o->default_value.s);
                                                o->default_value.s = strdup((char *) cur_node->children->content);
                                                break;
                                        case OPTION_TYPE_DOUBLE:
                                                o->default_value.d = atof((char *) cur_node->children->content);
                                                break;
                                        default:
                                                break;
                                }
                        } else if (std::string((char *) cur_node->name) == "min") {
                                if (!cur_node->children)
                                        continue;
                                if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
                                        printf("WARN: [%s] min defined for option type !int && !double\n", p->name);
                                o->data.min = atof((char *) cur_node->children->content);
                        } else if (std::string((char *) cur_node->name) == "max") {
                                if (!cur_node->children)
                                        continue;
                                if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
                                        printf("WARN: [%s] max defined for option type !int && !double\n", p->name);
                                o->data.max = atof((char *) cur_node->children->content);
                        } else if (std::string((char *) cur_node->name) == "precision") {
                                if (!cur_node->children)
                                        continue;
                                if (o->type != OPTION_TYPE_DOUBLE)
                                        printf("WARN: [%s] precision defined for option type !double\n", p->name);
                                o->data.precision = atof((char *) cur_node->children->content);
                        } else if (std::string((char *) cur_node->name) == "desc") {
                                if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_STRING)
                                        printf("WARN: [%s] desc defined for option type !int && !string\n", p->name);
                                xmlNode *node;
                                LabeledInt *li = NULL;
                                LabeledString *ls = NULL;
                                for (node = cur_node->children; node; node = node->next) {
                                        if (node->type == XML_ELEMENT_NODE) {
                                                if (o->type == OPTION_TYPE_INT) {
                                                        int is_value = (std::string((char *) node->name) == "value");
                                                        int is_name = (std::string((char *) node->name) == "_name");
                                                        if (!li && (is_value || is_name)) {
                                                                li = new LabeledInt();
                                                                o->int_labels.push_back(li);
                                                        }
                                                        if (is_value)
                                                                li->value = atoi((char *) node->children->content);
                                                        if (is_name)
                                                                li->name = strdup((char *) node->children->content);
                                                } else if (o->type == OPTION_TYPE_STRING) {
                                                        int is_value = (std::string((char *) node->name) == "value");
                                                        int is_name = (std::string((char *) node->name) == "_name");
                                                        if (!ls && (is_value || is_name)) {
                                                                ls = new LabeledString();
                                                                o->str_labels.push_back(ls);
                                                        }
                                                        if (is_value) {
                                                                ls->value = strdup((char *) node->children->content);
                                                                if (std::string(o->default_value.s) == "" && o->str_labels.size() == 1) {
                                                                        free(o->default_value.s);
                                                                        o->default_value.s = strdup(ls->value);
                                                                }
                                                        }
                                                        if (is_name)
                                                                ls->name = strdup((char *) node->children->content);
                                                }
                                        }
                                }
                        }
                }
                get_plugin_data(p, o, doc, cur_node->children);
        }
}

int
parse_xml_files(WCM *wcm, const char *dir_name)
{
        int len;
        DIR *dir;
        char *name;
        struct dirent *file;
        std::string path, filename;
        xmlDoc *doc = NULL;
        xmlNode *root_element = NULL;

        path = std::string(dir_name);
        dir = opendir((path).c_str());
        if (!dir) {
                printf("Error: Could not open %s\n", (path).c_str());
                return -1;
        }
        while ((file = readdir(dir))) {
                name = file->d_name;
                len = std::string(name).length();
                if (len > 3 && std::string(&name[len - 4]) == ".xml") {
                        filename = path + "/" + std::string(name);
                        doc = xmlReadFile(filename.c_str(), NULL, 0);
                        if (!doc) {
                                printf("Error: Could not parse file %s\n", filename.c_str());
                                continue;
                        }

                        root_element = xmlDocGetRootElement(doc);

                        if (root_element->type == XML_ELEMENT_NODE && std::string((char *) root_element->name) == "wayfire") {
                                printf("Loading plugin: %s\n", name);
                                Plugin *p = new Plugin();
                                p->category = strdup("Uncategorized");
                                p->wcm = wcm;
                                get_plugin_data(p, NULL, doc, root_element);
                                wcm->plugins.push_back(p);
                        }

                        xmlFreeDoc(doc);
                        xmlCleanupParser();
                }
        }

        closedir(dir);

        return 0;
}
