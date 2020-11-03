changequote(,)
changecom()dnl
define(RST_REF, [`$1 <$2.$1_>`_])
define(EREF, RST_REF($1, entity))
define(CREF, RST_REF($1, component))
define(UREF, RST_REF($1, usecase))
