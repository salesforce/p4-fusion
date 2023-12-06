#!/usr/bin/env bash

read -r -d '' license <<-"EOF"
/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
EOF

files=$(grep -rL "Copyright (c) 2022 Salesforce, Inc." * | grep "\.h\|\.cc")

for f in $files
do
  echo -e "$license" > temp
  cat $f >> temp
  mv temp $f
done
