/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "result.h"

void Result::HandleError(Error* e)
{
	StrBuf str;
	e->Fmt(&str);
	ERR("Received error: " << e->FmtSeverity() << " " << str.Text())
	m_Error = *e;
}
