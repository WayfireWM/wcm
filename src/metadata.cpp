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

static Option *
create_option(xmlNode *cur_node, Plugin *p)
{
        Option *o = new Option();
        xmlNode *node;
        xmlChar *prop;
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
        for (node = cur_node->children; node; node = node->next) {
                if (node->type != XML_ELEMENT_NODE)
                        continue;
                if (std::string((char *) node->name) == "_short") {
                        o->disp_name = strdup((char *) node->children->content);
                } else if (std::string((char *) node->name) == "default") {
                        if (!node->children)
                                continue;
                        switch (o->type) {
                                case OPTION_TYPE_INT:
                                        o->default_value.i = atoi((char *) node->children->content);
                                        break;
                                case OPTION_TYPE_BOOL:
                                        if (std::string((char *) node->children->content) == "true")
                                                o->default_value.i = 1;
                                        else if (std::string((char *) node->children->content) == "false")
                                                o->default_value.i = 0;
                                        else
                                                o->default_value.i = atoi((char *) node->children->content);
                                        if (o->default_value.i < 0 && o->default_value.i > 1)
                                                printf("WARN: [%s] unknown bool option default\n", p->name);
                                        break;
                                case OPTION_TYPE_STRING:
                                case OPTION_TYPE_BUTTON:
                                case OPTION_TYPE_COLOR:
                                case OPTION_TYPE_KEY:
                                        free(o->default_value.s);
                                        o->default_value.s = strdup((char *) node->children->content);
                                        break;
                                case OPTION_TYPE_DOUBLE:
                                        o->default_value.d = atof((char *) node->children->content);
                                        break;
                                default:
                                        break;
                        }
                } else if (std::string((char *) node->name) == "min") {
                        if (!node->children)
                                continue;
                        if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
                                printf("WARN: [%s] min defined for option type !int && !double\n", p->name);
                        o->data.min = atof((char *) node->children->content);
                } else if (std::string((char *) node->name) == "max") {
                        if (!node->children)
                                continue;
                        if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
                                printf("WARN: [%s] max defined for option type !int && !double\n", p->name);
                        o->data.max = atof((char *) node->children->content);
                } else if (std::string((char *) node->name) == "precision") {
                        if (!node->children)
                                continue;
                        if (o->type != OPTION_TYPE_DOUBLE)
                                printf("WARN: [%s] precision defined for option type !double\n", p->name);
                        o->data.precision = atof((char *) node->children->content);
                } else if (std::string((char *) node->name) == "desc") {
                        if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_STRING)
                                printf("WARN: [%s] desc defined for option type !int && !string\n", p->name);
                        xmlNode *n;
                        LabeledInt *li = NULL;
                        LabeledString *ls = NULL;
                        for (n = node->children; n; n = n->next) {
                                if (n->type == XML_ELEMENT_NODE) {
                                        if (o->type == OPTION_TYPE_INT) {
                                                int is_value = (std::string((char *) n->name) == "value");
                                                int is_name = (std::string((char *) n->name) == "_name");
                                                if (!li && (is_value || is_name)) {
                                                        li = new LabeledInt();
                                                        o->int_labels.push_back(li);
                                                }
                                                if (is_value)
                                                        li->value = atoi((char *) n->children->content);
                                                if (is_name)
                                                        li->name = strdup((char *) n->children->content);
                                        } else if (o->type == OPTION_TYPE_STRING) {
                                                int is_value = (std::string((char *) n->name) == "value");
                                                int is_name = (std::string((char *) n->name) == "_name");
                                                if (!ls && (is_value || is_name)) {
                                                        ls = new LabeledString();
                                                        o->str_labels.push_back(ls);
                                                }
                                                if (is_value) {
                                                        ls->value = strdup((char *) n->children->content);
                                                        if (std::string(o->default_value.s) == "" && o->str_labels.size() == 1) {
                                                                free(o->default_value.s);
                                                                o->default_value.s = strdup(ls->value);
                                                        }
                                                }
                                                if (is_name)
                                                        ls->name = strdup((char *) n->children->content);
                                        }
                                }
                        }
                }
        }

        return o;
}

static void
get_plugin_data(Plugin *p, Option *opt, Option *main_group, xmlDoc *doc, xmlNode * a_node)
{
        Option *o = opt;
        xmlNode *cur_node = NULL;
        xmlChar *prop;
        bool children_handled = false;

        for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;
                if (std::string((char *) cur_node->name) == "plugin") {
                        prop = xmlGetProp(cur_node, (xmlChar *) "name");
                        if (prop)
                                p->name = strdup((char *) prop);
                        free(prop);
                } else if (std::string((char *) cur_node->name) == "_short") {
                        p->disp_name = strdup((char *) cur_node->children->content);
                } else if (std::string((char *) cur_node->name) == "category") {
                        if (!cur_node->children)
                                continue;
                        free(p->category);
                        p->category = strdup((char *) cur_node->children->content);
                } else if (std::string((char *) cur_node->name) == "option") {
                        if (!main_group) {
                                main_group = new Option();
                                main_group->plugin = p;
                                main_group->name = strdup("General");
                                main_group->type = OPTION_TYPE_GROUP;
                                p->options.push_back(main_group);
                        }
                        o = create_option(cur_node, p);
                        children_handled = true;
                        main_group->options.push_back(o);
                } else if (std::string((char *) cur_node->name) == "group") {
                        xmlNode *node, *n;
                        Option *group = new Option();
                        group->plugin = p;
                        group->type = OPTION_TYPE_GROUP;
                        for (node = cur_node->children; node; node = node->next) {
                                if (node->type != XML_ELEMENT_NODE)
                                        continue;
                                if (std::string((char *) node->name) == "_short") {
                                        group->name = strdup((char *) node->children->content);
                                } else if (std::string((char *) node->name) == "option") {
                                        o = create_option(node, p);
                                        group->options.push_back(o);
                                } else if (std::string((char *) node->name) == "subgroup") {
                                        Option *subgroup = new Option();
                                        subgroup->plugin = p;
                                        subgroup->type = OPTION_TYPE_SUBGROUP;
                                        for (n = node->children; n; n = n->next) {
                                                if (n->type != XML_ELEMENT_NODE)
                                                        continue;
                                                if (std::string((char *) n->name) == "_short") {
                                                        subgroup->name = strdup((char *) n->children->content);
                                                } else if (std::string((char *) n->name) == "option") {
                                                        o = create_option(n, p);
                                                        subgroup->options.push_back(o);
                                                }
                                        }
                                        group->options.push_back(subgroup);
                                }
                        }
                        children_handled = true;
                        p->options.push_back(group);
                }
                if (!children_handled)
                        get_plugin_data(p, o, main_group, doc, cur_node->children);
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
                                get_plugin_data(p, NULL, NULL, doc, root_element);
                                wcm->plugins.push_back(p);
                        }

                        xmlFreeDoc(doc);
                        xmlCleanupParser();
                }
        }

        closedir(dir);

        return 0;
}
