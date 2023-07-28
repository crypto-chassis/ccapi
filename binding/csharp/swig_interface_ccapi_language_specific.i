%rename("%(camelcase)s", %$isfunction, %$ismember, %$ispublic) "";
%rename("%(camelcase)s", %$isfunction, %$ismember, %$isprotected) "";
%rename("_%(lowercamelcase)s", %$ismember, %$isprivate) "";
%rename("%(regex:/^(toString|getType|setType)$/\\u\\1_/)s") "";
