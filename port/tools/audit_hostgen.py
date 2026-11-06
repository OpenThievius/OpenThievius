# SPDX-License-Identifier: AGPL-3.0-or-later
"""Summarize a generated hostgen report without reading proprietary inputs."""
import argparse
import collections
import json
from pathlib import Path


def audit(report):
    units = []
    counts = collections.Counter()
    for unit in sorted(report.get('units_unreadable', [])):
        problems = report.get('problems', {}).get(unit, [])
        text = '\n'.join(problems)
        categories = []
        for needle, category in (
            ('duplicate member', 'inheritance/member collision'),
            ('negative size', 'layout assertion'),
            ('expected member name', 'unlowered C++ member'),
            ('too many arguments', 'call signature mismatch'),
            ('incompatible type', 'type mismatch'),
            ('undeclared', 'missing declaration'),
        ):
            if needle in text:
                categories.append(category)
                counts[category] += 1
        if not categories:
            categories = ['other compiler error']
            counts[categories[0]] += 1
        units.append({'unit': unit, 'categories': categories, 'diagnostics': problems})
    return {
        'note': 'Categories overlap; compiler diagnostics are authoritative. '
                'Direct-call frontier excludes unknown indirect targets.',
        'coverage': {key: report.get(key) for key in (
            'units', 'functions_translated', 'functions_from_assembly',
            'functions_stubbed', 'functions_without_c')},
        'unreadable_units': len(units),
        'category_counts': dict(sorted(counts.items())),
        'units': units,
        'missing_frontier': report.get('frontier', {}).get('missing', []),
        'arguments_filled_with_zero': report.get('arguments_filled_with_zero', {}),
        'call_site_mismatches': report.get('call_site_mismatches', {}),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = audit(json.loads(args.report.read_text()))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Unreadable units: {result['unreadable_units']}")
    for category, count in result['category_counts'].items():
        print(f'{count:3}  {category}')
    print(f'Audit: {args.out}')


if __name__ == '__main__':
    main()
