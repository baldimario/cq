<?php

require_once 'libcq.php';

$dataPath = dirname(__FILE__) . '/../../data/test_data.csv';

// test the API
$ctx = new CQContext();

$result = $ctx->execute("SELECT name FROM '$dataPath' LIMIT 5");

if ($result) {
    echo "Rows: " . $result->rows() . ", Cols: " . $result->cols() . "\n";
    for ($i = 0; $i < min(5, $result->rows()); $i++) {
        echo "Row $i: " . $result->get($i, 0) . "\n";
    }
} else {
    echo "Error: " . $ctx->error() . "\n";
}

// test builder with new features
$builder = new CQBuilder();
$builder->select('name');
$builder->select('age');
$builder->from($dataPath);
$builder->whereLike('name', 'A%');
$builder->orderBy('age', true);
$builder->limit(3);

$sql = $builder->toSql();
echo "Generated SQL: $sql\n";

$result2 = $builder->execute($ctx);
if ($result2) {
    echo "Builder result: Rows " . $result2->rows() . "\n";
} else {
    echo "Builder error: " . $ctx->error() . "\n";
}

// Test aggregates
$builder2 = new CQBuilder();
$builder2->count('name');
$builder2->sum('age');
$builder2->from($dataPath);

$sql2 = $builder2->toSql();
echo "Aggregate SQL: $sql2\n";

$result3 = $builder2->execute($ctx);
if ($result3) {
    echo "Aggregate result: Cols " . $result3->cols() . "\n";
    for ($j = 0; $j < $result3->cols(); $j++) {
        $colName = $result3->columnName($j);
        $val = $result3->get(0, $j);
        echo "  $colName: $val\n";
    }
} else {
    echo "Aggregate error: " . $ctx->error() . "\n";
}

// test alias and window
$builder3 = new CQBuilder();
$builder3->select('name', 'full_name');
$builder3->windowRowNumber(null, 'name');
$builder3->from($dataPath);
$builder3->limit(3);

$sql3 = $builder3->toSql();
echo "Alias/Window SQL: $sql3\n";

$result4 = $builder3->execute($ctx);
if ($result4) {
    echo "Window result: Rows " . $result4->rows() . "\n";
    for ($i = 0; $i < min($result4->rows(), 3); $i++) {
        $name = $result4->get($i, 0);
        $num = $result4->get($i, 1);
        echo "  $name: $num\n";
    }
} else {
    echo "Window error: " . $ctx->error() . "\n";
}

// test CASE
$builder4 = new CQBuilder();
$builder4->case_("age > 30", "'Senior'", "'Junior'");
$builder4->select('name', 'person');
$builder4->from($dataPath);
$builder4->limit(2);

$sql4 = $builder4->toSql();
echo "CASE SQL: $sql4\n";

$result5 = $builder4->execute($ctx);
if ($result5) {
    echo "CASE result: Rows " . $result5->rows() . "\n";
    for ($i = 0; $i < $result5->rows(); $i++) {
        $cat = $result5->get($i, 0);
        $name = $result5->get($i, 1);
        echo "  $cat: $name\n";
    }
} else {
    echo "CASE error: " . $ctx->error() . "\n";
}

// test subquery
$subBuilder = new CQBuilder();
$subBuilder->select('name');
$subBuilder->from($dataPath);
$subBuilder->whereLike('name', 'A%');

$builder5 = new CQBuilder();
$builder5->select('name');
$builder5->from($dataPath);
$builder5->whereInSubquery('name', $subBuilder);

$sql5 = $builder5->toSql();
echo "Subquery SQL: $sql5\n";

$result6 = $builder5->execute($ctx);
if ($result6) {
    echo "Subquery result: Rows " . $result6->rows() . "\n";
} else {
    echo "Subquery error: " . $ctx->error() . "\n";
}

// test window rank and date function
$builder6 = new CQBuilder();
$builder6->select('name');
$builder6->windowRank(null, 'name');
$builder6->from($dataPath);
$builder6->limit(3);

$sql6 = $builder6->toSql();
echo "Rank SQL: $sql6\n";

$result7 = $builder6->execute($ctx);
if ($result7) {
    echo "Rank result: Rows " . $result7->rows() . "\n";
    for ($i = 0; $i < $result7->rows(); $i++) {
        $name = $result7->get($i, 0);
        $rank = $result7->get($i, 1);
        echo "  $name: $rank\n";
    }
} else {
    echo "Rank error: " . $ctx->error() . "\n";
}

// test function
$builder7 = new CQBuilder();
$builder7->function_('UPPER', 'name');
$builder7->from($dataPath);
$builder7->limit(2);

$sql7 = $builder7->toSql();
echo "Function SQL: $sql7\n";

$result8 = $builder7->execute($ctx);
if ($result8) {
    echo "Function result: Rows " . $result8->rows() . "\n";
    for ($i = 0; $i < $result8->rows(); $i++) {
        $val = $result8->get($i, 0);
        echo "  $val\n";
    }
} else {
    echo "Function error: " . $ctx->error() . "\n";
}

// test arithmetic expressions
$builder9 = new CQBuilder();
$builder9->selectExpr('age * 2');
$builder9->selectExpr('age + 10 AS adjusted_age');
$builder9->from($dataPath);
$builder9->whereExpr('age / 2 > 15');
$builder9->limit(3);

$sql9 = $builder9->toSql();
echo "Arithmetic SQL: $sql9\n";

$result9 = $builder9->execute($ctx);
if ($result9) {
    echo "Arithmetic result: Rows " . $result9->rows() . ", Cols " . $result9->cols() . "\n";
    for ($i = 0; $i < $result9->rows(); $i++) {
        $val1 = $result9->get($i, 0);
        $val2 = $result9->get($i, 1);
        echo "  Row $i: $val1, $val2\n";
    }
} else {
    echo "Arithmetic error: " . $ctx->error() . "\n";
}

?>