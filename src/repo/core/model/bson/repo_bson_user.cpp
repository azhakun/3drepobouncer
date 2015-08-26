/**
*  Copyright (C) 2015 3D Repo Ltd
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

/**
*  User setting BSON
*/

#include "repo_bson_builder.h"
#include "repo_bson_user.h"


using namespace repo::core::model;

RepoUser::RepoUser() : RepoBSON()
{
}


RepoUser::~RepoUser()
{
}

std::list<std::pair<std::string, std::string> > RepoUser::getAPIKeysList() const
{

	RepoBSON customData = getCustomDataBSON();

	return customData.getListStringPairField(REPO_USER_LABEL_API_KEYS, REPO_USER_LABEL_LABEL, REPO_USER_LABEL_KEY);
}

std::vector<char> RepoUser::getAvatarAsRawData() const
{
	std::vector<char> image;
	RepoBSON customData = getCustomDataBSON();
	if (customData.hasField(REPO_USER_LABEL_AVATAR))
		getBinaryFieldAsVector(customData.getObjectField(REPO_USER_LABEL_AVATAR).getField("data"), &image);

	return image;
}

std::string RepoUser::getCleartextPassword() const
{
	std::string password;
	if (hasField(REPO_USER_LABEL_CREDENTIALS))
	{
		RepoBSON cred = getField(REPO_USER_LABEL_CREDENTIALS).embeddedObject();

		password = cred.getField(REPO_USER_LABEL_CLEARTEXT).str();
	}
	return password;
}

RepoBSON RepoUser::getCustomDataBSON() const
{
	RepoBSON customData;
	if (hasField(REPO_USER_LABEL_CUSTOM_DATA))
	{
		customData = getField(REPO_USER_LABEL_CUSTOM_DATA).embeddedObject();
	}
	return customData;
}

RepoBSON RepoUser::getRolesBSON() const
{
	RepoBSON roles;
	if (hasField(REPO_USER_LABEL_ROLES))
	{
		roles = getField(REPO_USER_LABEL_ROLES).embeddedObject();
	}
	return roles;
}

std::string RepoUser::getPassword() const
{
	std::string password;
	if (hasField(REPO_USER_LABEL_CREDENTIALS))
	{
		RepoBSON cred = getField(REPO_USER_LABEL_CREDENTIALS).embeddedObject();

		password = cred.getField(REPO_USER_LABEL_ENCRYPTION).str();
	}
	return password;
}

std::list<std::pair<std::string, std::string>>
	RepoUser::getRolesList() const
{
	std::list<std::pair<std::string, std::string>> result;
/*	RepoBSON roleData = getRolesBSON();
	if (!roleData.isEmpty())
		result = roleData.*/
	result = getListStringPairField(REPO_USER_LABEL_ROLES, REPO_USER_LABEL_DB, REPO_USER_LABEL_ROLE);

	return result;
}

std::list<std::pair<std::string, std::string>>
	RepoUser::getGroupsList() const
{
	std::list<std::pair<std::string, std::string>> result;
	RepoBSON customData = getCustomDataBSON();
	if (!customData.isEmpty())
		result = customData.getListStringPairField(REPO_USER_LABEL_GROUPS, REPO_USER_LABEL_OWNER, REPO_USER_LABEL_GROUP);

	return result;
}

std::list<std::pair<std::string, std::string>>
	RepoUser::getProjectsList() const 
{
	std::list<std::pair<std::string, std::string>> result;
	RepoBSON customData = getCustomDataBSON();
	if (!customData.isEmpty())
		result = customData.getListStringPairField(REPO_USER_LABEL_PROJECTS, REPO_USER_LABEL_OWNER, REPO_USER_LABEL_PROJECT);

	return result;
}
