/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "arguments.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <wordexp.h>

/**
 * Opens the specified file, removes lines beginning with # and parses the remaining contents with wordexp
 * @returns the vector of words as parsed by wordexp
 */
static std::unique_ptr<std::vector<std::string>> ArgsFromFile(const std::string& filename)
{
	std::ifstream infile(filename);
	if (!infile.is_open())
	{
		ERR("Failed to open file: " << filename);
		return nullptr;
	}
	std::string line;
	std::stringstream ss;
	while (std::getline(infile, line))
	{
		// Ignore empty lines and comments
		if (!line.empty() && line[0] != '#')
		{
			ss << line << " ";
		}
	}
	std::string fullcontent = ss.str();
	wordexp_t expRes;
	int resultCode = wordexp(fullcontent.c_str(), &expRes, WRDE_NOCMD);
	if (resultCode)
	{
		std::string errorMessage;
		switch (resultCode)
		{
			case WRDE_BADCHAR: errorMessage = "Illegal newline or |&;<>(){}"; break; 
			case WRDE_BADVAL: errorMessage = "An undefined shell variable was referenced"; break;
			case WRDE_CMDSUB: errorMessage = "Command substitution not allowed"; break;
			case WRDE_NOSPACE: errorMessage = "Out of memory"; break;
			case WRDE_SYNTAX: errorMessage = "Syntax error"; break;
			default: break;
		}
		ERR("Error parsing config file: " << errorMessage);
		return nullptr;
	}

	std::unique_ptr<std::vector<std::string>> args(new std::vector<std::string>());
	for (size_t i = 0; i < expRes.we_wordc; i++)
	{
		args->emplace_back(expRes.we_wordv[i]);
	}
	wordfree(&expRes);
	return args;
}

/**
 * Parses the specified config file and adds the arguments contained within to the parameter map as if they came from the command line
 */
void Arguments::AddArgumentsFromFile(const std::string& filename)
{
	std::unique_ptr<std::vector<std::string>> args = ArgsFromFile(filename);
	if (!args)
	{
		return;
	}

	for (int j = 0; j + 1 < args->size(); j += 2)
	{
		const std::string name2 = (*args)[j];
		const std::string value2 = (*args)[j + 1];
		const auto it2 = m_Parameters.find(name2);
		if (it2 != m_Parameters.end())
		{
			ParameterData& param2 = it2->second;
			param2.valueList.push_back(value2);
			param2.isSet = true;
		}
	}
}

void Arguments::Initialize(int argc, char** argv)
{
	m_Parameters.emplace("--config", ParameterData { false, false, {}, "Path to a file that can contain additional command line arguments." });
	for (int i = 1; i < argc - 1; i += 2)
	{
		const std::string name = argv[i];
		const std::string value = argv[i + 1];

		const auto it = m_Parameters.find(name);
		if (it != m_Parameters.end())
		{
			ParameterData& param = it->second;
			if (name == "--config")
			{
				AddArgumentsFromFile(value);
			}
			else
			{
				param.valueList.push_back(value);
				param.isSet = true;
			}
		}
		else
		{
			WARN("Unknown argument: " << name);
		}
	}
}

Arguments* Arguments::GetSingleton()
{
	static Arguments singleton;
	return &singleton;
}

std::string Arguments::GetParameter(const std::string& argName) const
{
	if (m_Parameters.find(argName) != m_Parameters.end())
	{
		if (m_Parameters.at(argName).valueList.empty())
		{
			return "";
		}
		// Use the last specified version of the parameter.
		return m_Parameters.at(argName).valueList.back();
	}
	return "";
}

std::vector<std::string> Arguments::GetParameterList(const std::string& argName) const
{
	if (m_Parameters.find(argName) != m_Parameters.end())
	{
		return m_Parameters.at(argName).valueList;
	}
	return {};
}

bool Arguments::IsValid() const
{
	for (auto& arg : m_Parameters)
	{
		const ParameterData& argData = arg.second;
		if (argData.isRequired && !argData.isSet)
		{
			return false;
		}
	}

	const bool hasAnyLFSParam = !GetLFSSpecs().empty() || !GetLFSServerUrl().empty() || !GetLFSUsername().empty() || !GetLFSPassword().empty();
	const bool hasAllLFSParams = !GetLFSSpecs().empty() && !GetLFSServerUrl().empty();
	if (hasAnyLFSParam && !hasAllLFSParams)
		return false;

	return true;
}

std::string Arguments::Help()
{
	std::string text = "\n";

	for (auto& param : m_Parameters)
	{
		const std::string& name = param.first;
		const ParameterData& paramData = param.second;

		text += name + " ";
		if (paramData.isRequired)
		{
			text += "\033[91m[Required]\033[0m";
		}
		else
		{
			text += "\033[93m[Optional, Default is " + (paramData.valueList.empty() ? "empty" : paramData.valueList.back()) + "]\033[0m";
		}
		text += "\n        " + paramData.helpText + "\n\n";
	}

	return text;
}

void Arguments::RequiredParameter(const std::string& name, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = true;
	paramData.isSet = false;
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}

void Arguments::OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = false;
	paramData.isSet = false;
	paramData.valueList.push_back(defaultValue);
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}

void Arguments::OptionalParameterList(const std::string& name, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = false;
	paramData.isSet = false;
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}
