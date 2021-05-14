#! /usr/bin/env sh

# Script for downloading some test data sets from the Chicago Mercantile
# Exchange FTP server.
#
# http://www.cmegroup.com/confluence/display/EPICSANDBOX/Market+Depth
#
# The test data from CME is not quite valid FIX. It has newlines in it and a
# strange "1128=9" prefix, so we do some slight transformations and then save
# a valid FIX test data file.

# Market Depth Futures E-Mini SP 500 (Dollar) 7/15/13
# ≅ 34MB zipped download
curl ftp://ftp.cmegroup.com/datamine_sample_data/md/mdff_cme_20130714-20130715_7817_0.zip | funzip | sed s/1128=9/8=FIX.4.2/ | tr -d '\n' > data/mdff_cme_20130714-20130715_7817_0.emini.sp500.future.fix

# Market Depth Options E-Mini SP 500 (Dollar) 7/15/13
# ≅ 302MB zipped download
curl ftp://ftp.cmegroup.com/datamine_sample_data/md/mdff_cme_20130714-20130715_7818_0.zip | funzip | sed s/1128=9/8=FIX.4.2/ | tr -d '\n' > data/mdff_cme_20130714-20130715_7818_0.emini.sp500.option.fix

