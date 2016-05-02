#!/bin/sh

if [ -d www ]
then
  rm -r wwwgz
  mkdir wwwgz
  if [ -d wwwgz ]
  then
    cd www

    for i in *
    do
      gzip -c $i >../wwwgz/$i
    done
    rm ../wwwgz/favicon.ico # not needed
  else
    echo "cannot create wwwgz directory"
  fi
else
  echo "www directory does not exist"
fi
