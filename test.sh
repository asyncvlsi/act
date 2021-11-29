#!/bin/bash

(cd ./act/transform/aflat/test/          && ./run.sh) &&
(cd ./act/transform/ext2sp/test/         && ./run.sh) &&
(cd ./act/transform/prs2cells/test/      && ./run.sh) &&
(cd ./act/transform/prs2net/test/        && ./run.sh) &&
(cd ./act/transform/prs2sim/test/        && ./run.sh) &&
(cd ./act/transform/testing/inline/test/ && ./run.sh) &&
(cd ./act/transform/testing/state/test/  && ./run.sh) &&
(cd ./act/transform/v2act/test/          && ./run.sh) &&
(cd ./act/lang/test/                     && ./run.sh)

