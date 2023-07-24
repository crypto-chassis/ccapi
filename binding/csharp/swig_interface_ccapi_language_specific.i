%rename("%(camelcase)s", %$isfunction, %$ismember, %$ispublic) "";
%rename("%(regex:/^(toString|getType|setType)$/\\u\\1_/)s") "";
