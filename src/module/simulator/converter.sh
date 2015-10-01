for f in moca.h moca_*
do
    sed -i 's/\#include <.*.h>//g' $f
done
