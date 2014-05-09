package main

import (
	"archive/tar"
	"bufio"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type ByteSize float64

const (
	_           = iota
	KB ByteSize = 1 << (10 * iota)
	MB
	GB
)

func (b ByteSize) String() string {
	switch {
	case b >= GB:
		return fmt.Sprintf("%.2fGB", b/GB)
	case b >= MB:
		return fmt.Sprintf("%.2fMB", b/MB)
	case b >= KB:
		return fmt.Sprintf("%.2fKB", b/KB)
	}
	return fmt.Sprintf("%.2fB", b)
}

type Consensus struct {
	Time time.Time
	Ids  []string
	Size int64
}

type Diff struct {
	Added   int
	Removed int
}

type Consensuses []Consensus

type StepStats struct {
	Diffs        []Diff
	MeanDiffSize ByteSize
}

type ConsensusStats struct {
	Data              Consensuses
	SS                []StepStats
	MeanEntrySize     ByteSize
	MeanConsensusSize ByteSize
	TotalSize         int64
	TotalEntryCount   int
}

func (c Consensuses) Len() int {
	return len(c)
}

func (c Consensuses) Less(i, j int) bool {
	return c[i].Time.Before(c[j].Time)
}

func (c Consensuses) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}

const (
	dateInForm  = "2006-01-02-15-04-05"
	dateOutForm = "2006-01-02-15-04-05"
)

func analyze(cs Consensuses, tr *tar.Reader) Consensuses {
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalln(err)
		}
		if hdr.Typeflag != tar.TypeReg {
			log.Println(hdr.Name)
			continue
		}

		strTime := path.Base(hdr.Name)[:len("YYYY-MM-DD-HH-MM-SS")]
		parsedTime, err := time.Parse(dateInForm, strTime)
		c := Consensus{Time: parsedTime, Size: hdr.Size}
		scanner := bufio.NewScanner(tr)
		for scanner.Scan() {
			text := scanner.Text()
			switch text[0] {
			case 'r':
				fields := strings.Split(text, " ")
				if len(fields) < 3 {
					continue
				}
				c.Ids = append(c.Ids, fields[2])
			default:
				continue
			}
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		cs = append(cs, c)
	}
	return cs
}

func results(cst ConsensusStats, steps int) {
	cs := cst.Data
	if len(cs) == 0 {
		return
	}
	sort.Sort(cs)
	cst.TotalSize = 0
	for _, c := range cs {
		cst.TotalSize += c.Size
		cst.TotalEntryCount += len(c.Ids)
	}
	cst.MeanConsensusSize = ByteSize(cst.TotalSize) / ByteSize(len(cs))
	cst.MeanEntrySize = ByteSize(cst.TotalSize) / ByteSize(cst.TotalEntryCount)
	fmt.Printf("== Global data ==\n")
	fmt.Printf("Number of consensuses: %d\n", len(cst.Data))
	fmt.Printf("Mean consensus size: %s\n", cst.MeanConsensusSize.String())
	fmt.Printf("Mean entry size: %s\n", cst.MeanEntrySize.String())
	cst.SS = make([]StepStats, steps)
	for step := 1; step <= steps; step++ {
		stepStats := cst.SS[step-1]
		diffs := stepStats.Diffs
		var totalDiffEntryCount int = 0
		for i, c := range cs {
			diff := Diff{}
			var prevIds []string
			prev := i - step
			if prev >= 0 {
				prevIds = cs[prev].Ids
			} else {
				prevIds = make([]string, 0)
			}
			prevIdsMap := make(map[string]bool)
			for _, id := range prevIds {
				prevIdsMap[id] = true
			}
			for _, id := range c.Ids {
				a, e := prevIdsMap[id]
				if e && a {
				} else {
					diff.Added++
				}
				delete(prevIdsMap, id)
			}
			diff.Removed += len(prevIdsMap)
			// Plus one to simulate overhead
			totalDiffEntryCount += diff.Added + diff.Removed + 1
			diffs = append(diffs, diff)
		}
		stepStats.MeanDiffSize = ByteSize(totalDiffEntryCount) * (cst.MeanEntrySize / ByteSize(len(diffs)))
		fmt.Printf("== Step %d ==\n", step)
		fmt.Printf("Mean diff size is %s\n", stepStats.MeanDiffSize.String())
	}
}

func main() {
	tarpaths, err := filepath.Glob("consensuses-*.tar.*")
	if err != nil {
		log.Fatal(err)
	}

	cst := ConsensusStats{}

	for _, tp := range tarpaths {
		fmt.Printf("Parsing %s\n", tp)
		ext := filepath.Ext(tp)
		var catcmd string
		switch ext {
		case ".xz":
			catcmd = "xzcat"
		case ".bz2":
			catcmd = "bzcat"
		case ".gz":
			catcmd = "zcat"
		default:
			log.Printf("Unsupported format .tar%s in %s", ext, tp)
			continue
		}
		cmd := exec.Command(catcmd, tp)
		fr, err := cmd.StdoutPipe()
		cmd.Stderr = os.Stderr
		if err != nil {
			log.Fatal(err)
		}
		defer fr.Close()
		if err := cmd.Start(); err != nil {
			log.Fatal(err)
		}
		tr := tar.NewReader(fr)
		cst.Data = analyze(cst.Data, tr)
	}
	results(cst, 10)
}
