/*
 * Copyright (c) 2021, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "arguments.h"

#include <algorithm>

void Arguments::Initialize(int argc, char** argv)
{
	for (int i = 1; i < argc - 1; i += 2)
	{
		std::string name = argv[i];

		if (m_Parameters.find(name) != m_Parameters.end())
		{
			m_Parameters.at(name).value = argv[i + 1];
			m_Parameters.at(name).isSet = true;
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
		return m_Parameters.at(argName).value;
	}
	return "";
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
	return true;
}

std::string Arguments::Help()
{
	std::string text = "\n";

	for (auto& param : m_Parameters)
	{
		const std::string& name = param.first;
		const ParameterData& paramData = param.second;

		if (paramData.isRequired)
		{
			text += "\033[91m[Required]\033[0m ";
		}
		else
		{
			text += "\033[93m[Optional, Default is " + (paramData.value.empty() ? "empty" : paramData.value) + "]\033[0m ";
		}
		text += name;
		text += "\n        " + paramData.helpText + "\n\n";
	}

	return text;
}

void Arguments::RequiredParameter(const std::string& name, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = true;
	paramData.isSet = false;
	paramData.value = "";
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}

void Arguments::OptionalParameter(const std::string& name, const std::string& defaultValue, const std::string& helpText)
{
	ParameterData paramData;
	paramData.isRequired = false;
	paramData.isSet = false;
	paramData.value = defaultValue;
	paramData.helpText = helpText;

	m_Parameters[name] = paramData;
}
