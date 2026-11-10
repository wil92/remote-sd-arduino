import os
import re
import sys
import minify_html

frontend_directory = 'frontend'
views_directory = 'lib/webserver/views'
check = '--check' in sys.argv
stale_views = []

base_c_header_file = r"""
#ifndef FILES_DEFININITION
#define FILES_DEFININITION

const char PAGE_CONTENT_VAR[] PROGMEM = R"PAGE(
HTML_CONTENT
)PAGE";

#endif //FILES_DEFININITION
"""

for filename in os.listdir(frontend_directory):
    if filename.endswith('.html'):
        filepath = os.path.join(frontend_directory, filename)
        with open(filepath, 'r') as file:
            with open(os.path.join(frontend_directory, 'common.css'), 'r') as stylesheet:
                page = file.read().replace('$$STYLES$$', stylesheet.read())
            content = minify_html.minify(page,
                                         minify_js=True,
                                         minify_css=True,
                                         keep_comments=False,
                                         keep_ssi_comments=False,
                                         remove_processing_instructions=True)
            # Substituted values may contain spaces and other unquoted-attribute
            # delimiters, even though the placeholders themselves do not.
            content = re.sub(
                r'value=(\$\$(?:SSID|PASSWORD|NAME)\$\$)(?=[\s>])',
                r'value="\1"', content)
            file_definition = '_'.join(re.split(r'[-.]', filename)).upper() + '_H'
            var_name = '_'.join(re.split(r'[-.]', filename)).lower()

            new_view_path = os.path.join(views_directory, filename.replace('.html', '.h'))
            file_content = base_c_header_file.replace('FILES_DEFININITION', file_definition)
            file_content = file_content.replace('HTML_CONTENT', content)
            file_content = file_content.replace('PAGE_CONTENT_VAR', var_name)
            if check:
                try:
                    with open(new_view_path, 'r') as existing_file:
                        if existing_file.read() != file_content:
                            stale_views.append(new_view_path)
                except FileNotFoundError:
                    stale_views.append(new_view_path)
            else:
                with open(new_view_path, 'w') as new_file:
                    new_file.write(file_content)
                print(f'Generated {new_view_path}')

if check:
    if stale_views:
        print('Stale generated views: ' + ', '.join(stale_views), file=sys.stderr)
        sys.exit(1)
    print('Generated views are up to date')
