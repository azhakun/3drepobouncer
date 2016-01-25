/**
*  Copyright (C) 2016 3D Repo Ltd
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU Affero General Public License as
*  published by the Free Software Foundation, either version 3 of the
*  License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Affero General Public License for more details.
*
*  You should have received a copy of the GNU Affero General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
//#include <boost/property_tree/json_parser.hpp>
#include "json_parser.h"
#include "../core/model/repo_node_utils.h"

namespace repo{
	namespace lib{
		class PropertyTree
		{
		public:
			PropertyTree();
			~PropertyTree();

			/**
			* Add an attribute to the field
			* This is very xml specific
			* it is to achieve things like <FIELD NAME="Hi">
			* instead of <FIELD>"Hi"</FIELD>
			* @param label field name to add to
			* @param attribute name of attribute
			* @param value value of attribute
			*/
			void addFieldAttribute(
				const std::string &label,
				const std::string &attribute,
				const std::string &value
				);

			/**
			* Add an attribute to the field with the a 3d vector as value
			* This is very xml specific
			* it is to achieve things like <FIELD NAME="Hi">
			* instead of <FIELD>"Hi"</FIELD>
			* @param label field name to add to
			* @param attribute name of attribute
			* @param value value of attribute
			*/
			void addFieldAttribute(
				const std::string  &label,
				const std::string  &attribute,
				const repo_vector_t &value
				);

			/**
			* Add a children onto the tree
			* label can denote it's inheritance.
			* for e.g. trunk.branch.leaf would denote the child name is
			* leaf, with parent branch and grandparent trunk.
			* @param label indicating where the child lives in the tree
			* @param value value of the children
			*/
			template <typename T>
			void addToTree(
				const std::string           &label,
				const T                     &value)
			{
				if (label.empty())
					tree.put(label, value);
				else
					tree.add(label, value);
			}

			/**
			* Add an array of values to the tree at a specified level
			* This is used for things like arrays within JSON
			* @param label indicating where the child lives in the tree
			* @param value vector of children to add
			*/
			template <typename T>
			void addToTree(
				const std::string           &label,
				const std::vector<T>        &value)
			{
				boost::property_tree::ptree arrayTree;
				for (const auto &child : value)
				{
					PropertyTree childTree;
					childTree.addToTree("", child);
					arrayTree.push_back(std::make_pair("", childTree.tree));
				}

				tree.add_child(label, arrayTree);
			}

			/**
			* Disable the JSON workaround for quotes
			* There is currently a work around to allow
			* json parser writes to remove quotes from
			* anything that isn't a string. Calling this 
			* function will disable it. You might want to do this
			* BEFORE you add any data into the tree for XML
			* or any non JSON exports
			*/
			void disableJSONWorkaround()
			{
				hackStrings = false;
			}

			/**
			* Merging sub tree into this tree
			* @param label label where the subTree goes
			* @param subTree the sub tree to add
			*/
			void mergeSubTree(
				const std::string &label,
				PropertyTree      &subTree)
			{
				tree.add_child(label, subTree.tree);
			}

			/**
			* Write tree data onto a stream
			* @param stream stream to write onto
			*/
			void write_json(
				std::iostream &stream
				) const
			{
				boost::property_tree::write_json(stream, tree);
			}

			/**
			* Write tree data onto a stream
			* @param stream stream to write onto
			*/
			void write_xml(
				std::iostream &stream
				) const
			{
				boost::property_tree::write_xml(stream, tree);
			}

		private:
			bool hackStrings;
			boost::property_tree::ptree tree;
		};

		// Template specialization
		template <>
		void PropertyTree::addToTree<std::string>(
			const std::string           &label,
			const std::string           &value);
	}
}