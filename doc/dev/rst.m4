changequote(,)
changecom()dnl
define(RST_REF, [`$1 <$2.$1_>`_])
define(EREF, RST_REF($1, entity))
define(CREF, RST_REF($1, component))
define(UREF, RST_REF($1, usecase))
define(SRC2, https://github.com/Seagate/cortx-motr/blob/$2/$1)
define(SRC, SRC2($1, dev))
define(CODE, :code:`$1` [`search <https://github.com/search?p=1&q=$1+repo%3ASeagate%2Fcortx-motr&type=Code>`__])
