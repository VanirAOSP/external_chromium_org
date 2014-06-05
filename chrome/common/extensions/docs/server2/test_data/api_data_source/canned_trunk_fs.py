# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json


CANNED_TRUNK_FS_DATA = {
  'api': {
    '_api_features.json': json.dumps({
      'add_rules_tester': { 'dependencies': ['permission:add_rules_tester'] },
      'ref_test': { 'dependencies': ['permission:ref_test'] },
      'tester': { 'dependencies': ['permission:tester', 'manifest:tester'] }
    }),
    '_manifest_features.json': '{}',
    '_permission_features.json': '{}',
    'add_rules_tester.json': json.dumps([{
      'namespace': 'add_rules_tester',
      'description': ('A test api with a couple of events which support or '
                      'do not support rules.'),
      'types': [],
      'functions': [],
      'events': [
        {
          'name': 'rules',
          'options': {
            'supportsRules': True,
            'conditions': [],
            'actions': []
          }
        },
        {
          'name': 'noRules',
          'type': 'function',
          'description': 'Listeners can be registered with this event.',
          'parameters': []
        }
      ]
    }]),
    'tester.json': json.dumps([{
      'namespace': 'tester',
      'description': 'a test api',
      'types': [
        {
          'id': 'TypeA',
          'type': 'object',
          'description': 'A cool thing.',
          'properties': {
            'a': {'nodoc': True, 'type': 'string', 'minimum': 0},
            'b': {'type': 'array', 'optional': True, 'items': {'$ref': 'TypeA'},
                  'description': 'List of TypeA.'}
          }
        }
      ],
      'functions': [
        {
          'name': 'get',
          'type': 'function',
          'description': 'Gets stuff.',
          'parameters': [
            {
              'name': 'a',
              'description': 'a param',
              'choices': [
                {'type': 'string'},
                {'type': 'array', 'items': {'type': 'string'}, 'minItems': 1}
              ]
            },
            {
              'type': 'function',
              'name': 'callback',
              'parameters': [
                {'name': 'results', 'type': 'array', 'items': {'$ref': 'TypeA'}}
              ]
            }
          ]
        }
      ],
      'events': [
        {
          'name': 'EventA',
          'type': 'function',
          'description': 'A cool event.',
          'parameters': [
            {'type': 'string', 'name': 'id'},
            {
              '$ref': 'TypeA',
              'name': 'bookmark'
            }
          ]
        }
      ]
    }]),
    'ref_test.json': json.dumps([{
      'namespace': 'ref_test',
      'description': 'An API for testing ref\'s',
      'types': [
        {
          'id': 'type1',
          'type': 'string',
          'description': '$ref:type2'
        },
        {
          'id': 'type2',
          'type': 'string',
          'description': 'A $ref:type3, or $ref:type2'
        },
        {
          'id': 'type3',
          'type': 'string',
          'description': '$ref:other.type2 != $ref:ref_test.type2'
        }
      ],
      'events': [
        {
          'name': 'event1',
          'type': 'function',
          'description': 'We like $ref:type1',
          'parameters': [
            {
              'name': 'param1',
              'type': 'string'
            }
          ]
        }
      ],
      'properties': {
        'prop1': {
          '$ref': 'type3'
        }
      },
      'functions': [
        {
          'name': 'func1',
          'type': 'function',
          'parameters': [
            {
              'name': 'param1',
              'type': 'string'
            }
          ]
        }
      ]
    }])
  },
  'docs': {
    'templates': {
      'intros': {
        'test.html': '<h1>hi</h1>you<h2>first</h2><h3>inner</h3><h2>second</h2>'
      },
      'json': {
        'api_availabilities.json': json.dumps({
          'trunk_api': {
            'channel': 'trunk'
          },
          'dev_api': {
            'channel': 'dev'
          },
          'beta_api': {
            'channel': 'beta'
          },
          'stable_api': {
            'channel': 'stable',
            'version': 20
          }
        }),
        'intro_tables.json': json.dumps({
          'tester': {
            'Permissions': [
              {
                'class': 'override',
                'text': '"tester"'
              },
              {
                'text': 'is an API for testing things.'
              }
            ],
            'Learn More': [
              {
                'link': 'https://tester.test.com/welcome.html',
                'text': 'Welcome!'
              }
            ]
          }
        })
      },
      'private': {
        'intro_tables': {
          'trunk_message.html': 'available on trunk'
        }
      }
    }
  }
}
