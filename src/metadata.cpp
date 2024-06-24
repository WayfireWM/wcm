/*
 * Copyright © 2020 Scott Moreau
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
 *
 */

#include "wcm.hpp"
#include <stdio.h>
#include <wayfire/config/xml.hpp>

Option::Option(xmlNode *cur_node, Plugin *plugin)
{
    xmlNode *node;
    xmlChar *prop;
    this->plugin = plugin;
    prop = xmlGetProp(cur_node, (xmlChar*)"name");
    if (prop)
    {
        this->name = (char*)prop;
    }

    free(prop);
    prop = xmlGetProp(cur_node, (xmlChar*)"type");
    if (prop)
    {
        std::string type = (char*)prop;
        if (type == "int")
        {
            this->type     = OPTION_TYPE_INT;
            this->data.min = -DBL_MAX;
            this->data.max = DBL_MAX;
        } else if (type == "double")
        {
            this->type     = OPTION_TYPE_DOUBLE;
            this->data.min = -DBL_MAX;
            this->data.max = DBL_MAX;
            this->data.precision = 0.1;
        } else if (type == "bool")
        {
            this->type = OPTION_TYPE_BOOL;
        } else if (type == "string")
        {
            this->type = OPTION_TYPE_STRING;
            this->default_value = "";
            this->data.hints    = (hint_type)0;
        } else if (type == "button")
        {
            this->type = OPTION_TYPE_BUTTON;
            this->default_value = "";
        } else if (type == "gesture")
        {
            this->type = OPTION_TYPE_GESTURE;
            this->default_value = "";
        } else if (type == "activator")
        {
            this->type = OPTION_TYPE_ACTIVATOR;
            this->default_value = "";
        } else if (type == "color")
        {
            this->type = OPTION_TYPE_COLOR;
            this->default_value = "";
        } else if (type == "key")
        {
            this->type = OPTION_TYPE_KEY;
            this->default_value = "";
        } else if (type == "dynamic-list")
        {
            this->type = OPTION_TYPE_DYNAMIC_LIST;
        } else if (type == "animation")
        {
            this->type     = OPTION_TYPE_ANIMATION;
            this->data.min = 0;
            this->data.max = DBL_MAX;
        } else
        {
            printf("WARN: [%s] unknown option type: %s\n", plugin->name.c_str(),
                prop);
            this->type = OPTION_TYPE_UNDEFINED;
        }
    } else
    {
        printf("WARN: [%s] no option type found\n", plugin->name.c_str());
        this->type = OPTION_TYPE_UNDEFINED;
    }

    free(prop);
    prop = xmlGetProp(cur_node, (xmlChar*)"hidden");
    if (prop && (std::string((char*)prop) == "true"))
    {
        this->hidden = true;
    }

    free(prop);
    for (node = cur_node->children; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        std::string node_name = (char*)node->name;
        if (node_name == "_short")
        {
            this->disp_name = (char*)node->children->content;
        } else if (node_name == "_long")
        {
            this->tooltip = (char*)node->children->content;
        } else if (node_name == "default")
        {
            if (!node->children)
            {
                continue;
            }

            switch (this->type)
            {
              case OPTION_TYPE_INT:
                this->default_value = atoi((char*)node->children->content);
                break;

              case OPTION_TYPE_ANIMATION:
                this->default_value = (char*)node->children->content;
                break;

              case OPTION_TYPE_BOOL:
                if (std::string((char*)node->children->content) == "true")
                {
                    this->default_value = 1;
                } else if (std::string((char*)node->children->content) == "false")
                {
                    this->default_value = 0;
                } else
                {
                    this->default_value = atoi((char*)node->children->content);
                }

                if ((std::get<int>(this->default_value) < 0) &&
                    (std::get<int>(this->default_value) > 1))
                {
                    printf("WARN: [%s] unknown bool option default\n",
                        plugin->name.c_str());
                }

                break;

              case OPTION_TYPE_ACTIVATOR:
              case OPTION_TYPE_GESTURE:
              case OPTION_TYPE_STRING:
              case OPTION_TYPE_BUTTON:
              case OPTION_TYPE_COLOR:
              case OPTION_TYPE_KEY:
                this->default_value = (char*)node->children->content;
                break;

              case OPTION_TYPE_DOUBLE:
                this->default_value = atof((char*)node->children->content);
                break;

              default:
                break;
            }
        } else if ((node_name == "type") && (this->type == OPTION_TYPE_DYNAMIC_LIST))
        {
            char *type = (char*)node->children->content;
            this->default_value = type;
        } else if (node_name == "min")
        {
            if (!node->children)
            {
                continue;
            }

            if ((this->type != OPTION_TYPE_INT) &&
                (this->type != OPTION_TYPE_DOUBLE) &&
                (this->type != OPTION_TYPE_ANIMATION))
            {
                printf("WARN: [%s] min defined for option type !int && !double\n",
                    plugin->name.c_str());
            }

            this->data.min = atof((char*)node->children->content);
        } else if (node_name == "max")
        {
            if (!node->children)
            {
                continue;
            }

            if ((this->type != OPTION_TYPE_INT) &&
                (this->type != OPTION_TYPE_DOUBLE) &&
                (this->type != OPTION_TYPE_ANIMATION))
            {
                printf("WARN: [%s] max defined for option type !int && !double\n",
                    plugin->name.c_str());
            }

            this->data.max = atof((char*)node->children->content);
        } else if (node_name == "precision")
        {
            if (!node->children)
            {
                continue;
            }

            if (this->type != OPTION_TYPE_DOUBLE)
            {
                printf("WARN: [%s] precision defined for option type !double\n",
                    plugin->name.c_str());
            }

            this->data.precision = atof((char*)node->children->content);
        } else if (node_name == "hint")
        {
            if (!node->children)
            {
                continue;
            }

            if ((this->type != OPTION_TYPE_STRING) &&
                (this->type != OPTION_TYPE_DYNAMIC_LIST))
            {
                printf("WARN: [%s] hint defined for option type !string\n",
                    plugin->name.c_str());
            }

            if (std::string((char*)node->children->content) == "file")
            {
                this->data.hints = (hint_type)(this->data.hints | HINT_FILE);
            }

            if (std::string((char*)node->children->content) == "directory")
            {
                this->data.hints = (hint_type)(this->data.hints | HINT_DIRECTORY);
            }
        } else if (node_name == "desc")
        {
            if ((this->type != OPTION_TYPE_INT) &&
                (this->type != OPTION_TYPE_STRING))
            {
                printf("WARN: [%s] desc defined for option type !int && !string\n",
                    plugin->name.c_str());
            }

            xmlNode *n;
            std::unique_ptr<LabeledInt> li    = nullptr;
            std::unique_ptr<LabeledString> ls = nullptr;
            for (n = node->children; n; n = n->next)
            {
                if (n->type == XML_ELEMENT_NODE)
                {
                    if (this->type == OPTION_TYPE_INT)
                    {
                        int is_value = (std::string((char*)n->name) == "value");
                        int is_name  = (std::string((char*)n->name) == "_name");
                        if (!li && (is_value || is_name))
                        {
                            li = std::make_unique<LabeledInt>();
                        }

                        if (is_value)
                        {
                            li->value = atoi((char*)n->children->content);
                        }

                        if (is_name)
                        {
                            li->name = strdup((char*)n->children->content);
                        }
                    } else if (this->type == OPTION_TYPE_STRING)
                    {
                        int is_value = (std::string((char*)n->name) == "value");
                        int is_name  = (std::string((char*)n->name) == "_name");
                        if (!ls && (is_value || is_name))
                        {
                            ls = std::make_unique<LabeledString>();
                        }

                        if (is_value)
                        {
                            ls->value = (char*)n->children->content;
                            if (std::get<std::string>(this->default_value).empty() &&
                                (this->str_labels.size() == 1))
                            {
                                this->default_value = ls->value;
                            }
                        }

                        if (is_name)
                        {
                            ls->name = (char*)n->children->content;
                        }
                    }
                }
            }

            if (li)
            {
                std::pair<std::string, int> pair(li->name, li->value);
                int_labels.push_back(pair);
            }

            if (ls)
            {
                std::pair<std::string, std::string> pair(ls->name, ls->value);
                str_labels.push_back(pair);
            }
        }
    }
}

Option::Option(option_type group_type, Plugin *plugin) : plugin(plugin), type(
        group_type)
{}

Option::~Option()
{
    if (parent)
    {
        auto & children = parent->options;
        children.erase(std::remove(children.begin(),
            children.end(), this), children.end());
    }
}

Option*Option::create_child_option(const std::string & name, option_type type)
{
    auto *option = new Option();
    option->name   = name;
    option->parent = this;
    options.push_back(option);
    option->plugin = plugin;
    option->type   = type;
    return option;
}

Plugin*Plugin::get_plugin_data(xmlNode *cur_node, Option *main_group, Plugin *plugin)
{
    xmlChar *prop;
    bool children_handled = false;

    for (; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        std::string cur_node_name = (char*)cur_node->name;
        if (cur_node_name == "object")
        {
            return nullptr;
        }

        if (cur_node_name == "plugin")
        {
            plugin = new Plugin();
            plugin->category = "Uncategorized";
            prop = xmlGetProp(cur_node, (xmlChar*)"name");
            if (prop)
            {
                plugin->name = (char*)prop;
            }

            free(prop);
        } else if (cur_node_name == "_short")
        {
            plugin->disp_name = (char*)cur_node->children->content;
        } else if (cur_node_name == "_long")
        {
            plugin->tooltip = (char*)cur_node->children->content;
        } else if (cur_node_name == "category")
        {
            if (!cur_node->children)
            {
                continue;
            }

            plugin->category = (char*)cur_node->children->content;
        } else if (cur_node_name == "option")
        {
            if (!main_group)
            {
                main_group = new Option(OPTION_TYPE_GROUP, plugin);
                main_group->name = "General";
                plugin->option_groups.push_back(main_group);
            }

            children_handled = true;
            main_group->options.push_back(new Option(cur_node, plugin));
        } else if (cur_node_name == "group")
        {
            xmlNode *node;
            Option *group = new Option(OPTION_TYPE_GROUP, plugin);
            for (node = cur_node->children; node; node = node->next)
            {
                if (node->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                std::string node_name = (char*)node->name;
                if (node_name == "_short")
                {
                    group->name = (char*)node->children->content;
                } else if (node_name == "option")
                {
                    group->options.push_back(new Option(node, plugin));
                } else if (node_name == "subgroup")
                {
                    Option *subgroup = new Option(OPTION_TYPE_SUBGROUP, plugin);
                    for (xmlNode *n = node->children; n; n = n->next)
                    {
                        if (n->type != XML_ELEMENT_NODE)
                        {
                            continue;
                        }

                        if (std::string((char*)n->name) == "_short")
                        {
                            subgroup->name = (char*)n->children->content;
                        } else if (std::string((char*)n->name) == "option")
                        {
                            subgroup->options.push_back(new Option(n, plugin));
                        }
                    }

                    group->options.push_back(subgroup);
                }
            }

            children_handled = true;
            plugin->option_groups.push_back(group);
        }

        if (!children_handled)
        {
            plugin = get_plugin_data(cur_node->children, main_group, plugin);
        }
    }

    return plugin;
}
